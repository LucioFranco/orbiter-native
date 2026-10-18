// Copyright (c) Orbiter contributors
// Licensed under the MIT License
// Native application host for the existing Orbiter world. No alternate physics.
#include "Orbiter.h"
#include "CoreState.h"
#include "D3dmath.h"
#include "State.h"
#include "Psys.h"
#include "Vessel.h"
#include "Camera.h"
#include "NativeCockpit.h"
#include "NativeMFD.h"
#include "NativeSound.h"
#include "imgui.h"
#include "Pane.h"
#include "Log.h"
#include "Platform/ModuleLibrary.h"
#include "Platform/Vfs.h"
#include "Platform/Joystick.h"
#include <memory>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace orbiter {void CreateNativeImGuiContext();}

Orbiter *g_pOrbiter = nullptr;
PlanetarySystem *g_psys = nullptr;
Camera *g_camera = nullptr;
Pane *g_pane = nullptr;
Vessel *g_focusobj = nullptr;
Vessel *g_pfocusobj = nullptr;
Select *g_select = nullptr;
InputBox *g_input = nullptr;
bool g_bStateUpdate = false;
bool g_bForceUpdate = false;

Orbiter::Orbiter()
    : memstat(nullptr), simheapsize(0), FRsysname(nullptr), FRsys_stream(nullptr),
      frec_sys_simt(0), FReditor(nullptr), tjump{}, pConfig(nullptr), pState(nullptr),
      m_pLaunchpad(nullptr), pDlgMgr(nullptr), m_pConsole(nullptr), pDI(nullptr),
      hInst(nullptr), hRenderWnd(nullptr), hBk(nullptr), bRenderOnce(false),
      bEnableLighting(true), bUseStencil(false), bKeepFocus(false), bStartVideoTab(false),
      bWINEenv(false), ignorefirst(false), plZ4(0), video_skip_count(0), snote(nullptr),
      nsnote(0), snote_playback(nullptr), script(nullptr), hScnInterp(nullptr),
      bFullscreen(false), viewW(0), viewH(0), viewBPP(32), cfgpath{}, cfglen(0), simkstate{},
      bActive(false), bAllowInput(false), bVisible(false), bRunning(false),
      bRequestRunning(false), bSession(false), bEnableAtt(true), bRecord(false),
      bPlayback(false), bCapture(false), bFastExit(false), bSysClearType(false),
      bRoughType(false), ctrlJoystick{}, ctrlKeyboard{}, ctrlTotal{},
      register_module(nullptr), customcmd(nullptr), ncustomcmd(0), gclient(nullptr), oclient(nullptr)
{
    if (g_pOrbiter) throw std::logic_error("Only one Orbiter world may be active");
    auto config = std::make_unique<Config>();
    auto state = std::make_unique<State>();
    pConfig = config.release();
    pState = state.release();
    g_pOrbiter = this;
    D3DMathSetup();
    orbiter::CreateNativeImGuiContext();
    orbiter::NativeMFDInit();orbiter::NativeDialogsReset();
}

Orbiter::~Orbiter()
{
    orbiter::NativeCockpitState().Reset();
    orbiter::NativeMFDShutdown();
    if (bSession)
        for (auto &plugin : m_Plugin) plugin.pModule->clbkSimulationEnd();
    FRecorder_ClosePlayback();
    for (DWORD i=0; i<nsnote; ++i) delete snote[i];
    delete []snote; snote=nullptr; nsnote=0;
    if (gclient) gclient->clbkCloseSession(false);
    delete g_psys;
    g_psys = nullptr;
    delete g_camera;
    g_camera = nullptr;
    g_focusobj = g_pfocusobj = nullptr;
    meshmanager.Flush();
    while (!m_Plugin.empty()) UnloadModule(m_Plugin.back().hDLL);
    orbiter::NativeMFDReleaseRegistry();
    delete g_select;g_select=nullptr;delete g_input;g_input=nullptr;
    delete pState;
    delete pConfig;
    ImGui::DestroyContext();
    g_pOrbiter = nullptr;
}

void Orbiter::OutputLoadStatus(const char *message, int)
{
    std::cout << message << '\n';
}

void Orbiter::TerminateOnError()
{
    throw std::runtime_error("Orbiter could not initialize the world; see the log for details");
}

void Orbiter::UpdateDeallocationProgress()
{
    // There is no Launchpad progress window in the native host.
}

bool Orbiter::InitializeWorld(char *name)
{
    if (g_psys) throw std::logic_error("A world is already loaded");
    g_camera = new Camera(1.0, 1.0e8);
    g_camera->ResizeViewport(1280, 720);
    auto report = [](const char *message, int line, void *context) {
        static_cast<Orbiter *>(context)->OutputLoadStatus(message, line);
    };
    g_psys = new PlanetarySystem(name, pConfig, report, this);
    return g_psys->nObj() != 0;
}

void Orbiter::Launch(const char *scenario)
{
    if (bSession || g_psys) throw std::logic_error("A scenario is already loaded");
    pConfig->Load("Orbiter.cfg"); // Missing file uses the normal Config defaults.
    const std::string path = ScnPath(scenario);
    if (!pState->Read(path.c_str())) throw std::runtime_error("Cannot read scenario: " + path);
    RigidBody::GlobalSetup();
    td.Reset(pState->Mjd());
    if (!InitializeWorld(pState->Solsys())) TerminateOnError();
    g_psys->InitState(path.c_str());
    if (!g_psys->nVessel()) throw std::runtime_error("Scenario contains no vessels: " + path);
    auto focus = g_psys->GetVessel(pState->Focus());
    if (!focus) focus = g_psys->GetVessel(DWORD{0});
    SetFocusObject(focus, false);
    g_camera->InitState(scenario, focus);
    FRecorder_Reset();
    bSession = bRunning = bRequestRunning = true;
    orbiter::PauseNativeAudio(false);
    g_bForceUpdate = true;
    g_psys->PostCreation();
    orbiter::NativeMFDRead(path);
    for (auto &plugin : m_Plugin) plugin.pModule->clbkSimulationStart(oapi::Module::RENDER_NONE);
    if (pConfig->CfgLogicPrm.bStartPaused) {
        // Upstream initialises the world once before requesting pause. Its
        // startup interval is 10 ms unless a configured fixed step overrides it.
        const double initialStep = pConfig->CfgCmdlinePrm.FixedStep > 0.0
            ? pConfig->CfgCmdlinePrm.FixedStep
            : pConfig->CfgDebugPrm.FixedStep > 0.0 ? pConfig->CfgDebugPrm.FixedStep : 0.01;
        StepSimulation(initialStep);
        Pause(true); // The existing next-tick boundary applies the pause callback.
    }
}

void Orbiter::StepSimulation(double seconds)
{
    if (!bSession) throw std::logic_error("Load a scenario before stepping");
    if (!(seconds > 0) || !std::isfinite(seconds)) throw std::invalid_argument("Invalid timestep");
    if(bRunning!=bRequestRunning) {
        bRunning=bRequestRunning;
        orbiter::PauseNativeAudio(!bRunning);
        ReleaseNativeKeys();
        ReleaseNativeCockpitMouse();
        for(auto &plugin:m_Plugin)plugin.pModule->clbkPause(!bRunning);
    }
    StepNativeCockpitMouse();
    td.BeginStep(seconds, bRunning);
    std::fill(std::begin(ctrlKeyboard),std::end(ctrlKeyboard),0);
    std::fill(std::begin(ctrlTotal),std::end(ctrlTotal),0);
    if (bRunning && g_focusobj) {
        char keys[256];
        std::copy(std::begin(simkstate), std::end(simkstate), keys);
        KbdInputImmediate_OnRunning(keys);
        std::copy(std::begin(ctrlKeyboard),std::end(ctrlKeyboard),ctrlTotal);
        g_focusobj->ApplyUserAttitudeControls(ctrlTotal);
    }
    if (bRunning) ModulePreStep();
    g_bStateUpdate = true;
    try {
        if (bRunning && td.SimDT) g_psys->Update(g_bForceUpdate);
        if (bRunning) ModulePostStep();
        if (bRunning) g_psys->FinaliseUpdate();
    } catch (...) {
        g_bStateUpdate = false;
        throw;
    }
    g_bStateUpdate = false;
    g_bForceUpdate = false;
    td.EndStep(bRunning);
    if (!KillVessels()) {
        bRunning = bRequestRunning = false;
        throw std::runtime_error("Cannot delete the last available focus vessel");
    }
    if (g_camera->Target()) g_camera->Update();
    // Update renderer-owned simulation state once per tick, including paused
    // ticks. Rendering may happen more or less often than simulation stepping.
    if (auto graphics=GetGraphicsClient()) graphics->clbkUpdate(bRunning);
    orbiter::UpdateNativeAudio();
}

void Orbiter::Pause(bool pause)
{
    // Apply at the next tick boundary, including requests made from callbacks.
    if(bSession)bRequestRunning=!pause;
}

DLLEXPORT void oapiSetPause(bool pause) {g_pOrbiter->Pause(pause);}

HINSTANCE Orbiter::LoadModule(const char *path, const char *name)
{
    using namespace orbiter::platform;
    auto filename = NormalizePath(path) / (std::string(name) + ModuleSuffix());
    if (!std::filesystem::exists(ResolveModulePath(filename.string())))
        filename = NormalizePath(path) / name / (std::string(name) + ModuleSuffix());
    auto previousRegistration = register_module;
    register_module = nullptr;
    HINSTANCE handle = nullptr;
    try { handle = OpenModule(filename.string()); }
    catch (...) { register_module = previousRegistration; throw; }
    auto module = register_module;
    register_module = previousRegistration;
    if (!handle) { LOGOUT_ERR("%s", LibraryError().c_str()); return nullptr; }
    m_Plugin.push_back({handle, module ? module : new oapi::Module(handle), name, !module});
    return handle;
}

bool Orbiter::UnloadModule(HINSTANCE handle)
{
    for (auto it = m_Plugin.begin(); it != m_Plugin.end(); ++it) {
        if (it->hDLL != handle) continue;
        auto module = *it;
        m_Plugin.erase(it);
        if (module.bLocalAlloc) delete module.pModule;
        orbiter::platform::CloseModule(handle);
        return true;
    }
    return false;
}

bool Orbiter::UnloadModule(const std::string &name)
{
    for (const auto &module : m_Plugin)
        if (!_stricmp(name.c_str(), module.sName.c_str())) return UnloadModule(module.hDLL);
    return false;
}

Vessel *Orbiter::SetFocusObject(Vessel *vessel, bool setview)
{
    if (!vessel || vessel == g_focusobj) return nullptr;
    const bool cockpit=setview && bSession && g_camera && g_camera->IsInternal() &&
        g_camera->GetIntMode()==CAMERA_VIRTUALCOCKPIT;
    ReleaseNativeKeys();
    if(g_focusobj && bSession) {
        char released[256]{};
        g_focusobj->ConsumeDirectKey(released);
    }
    std::fill(std::begin(ctrlKeyboard),std::end(ctrlKeyboard),0);
    std::fill(std::begin(ctrlTotal),std::end(ctrlTotal),0);
    ReleaseNativeCockpitMouse();
    orbiter::NativeCockpitState().Reset();
    orbiter::NativeDialogsReset();
    g_pfocusobj = g_focusobj;
    g_focusobj = vessel;
    if (g_pane) g_pane->FocusChanged(vessel);
    if (setview) SetView(vessel, 2);
    if (g_pfocusobj) g_pfocusobj->FocusChanged(false, vessel, g_pfocusobj);
    vessel->FocusChanged(true, vessel, g_pfocusobj);
    for (auto &plugin : m_Plugin) plugin.pModule->clbkFocusChanged(vessel, g_pfocusobj);
    if(cockpit)LoadNativeVirtualCockpit();
    orbiter::UpdateNativeAudio();
    return g_pfocusobj;
}

bool Orbiter::CycleNativeFocus()
{
    if(!bSession || !g_psys || !g_focusobj)return false;
    const DWORD count=g_psys->nVessel();
    DWORD current=0;
    while(current<count && g_psys->GetVessel(current)!=g_focusobj)++current;
    for(DWORD offset=1;offset<count;++offset) {
        auto next=g_psys->GetVessel((current+offset)%count);
        if(next->GetEnableFocus()) {SetFocusObject(next);return true;}
    }
    return false;
}

void Orbiter::SetView(Body *body, int mode)
{
    g_camera->Attach(body, mode);
    g_bForceUpdate = true;
}

void Orbiter::InsertVessel(Vessel *vessel)
{
    g_psys->AddVessel(vessel);
    for (auto &plugin : m_Plugin) plugin.pModule->clbkNewVessel(vessel);
    if (gclient) gclient->clbkNewVessel(vessel);
    vessel->PostCreation();
    vessel->InitSupervessel();
    vessel->ModulePostCreation();
}

void Orbiter::NotifyObjectJump(const Body *object, const Vector &shift)
{
    if (g_camera) {
        if (object == g_camera->Target()) g_camera->Drag(-shift);
        if (g_camera->Target()) g_camera->Update();
    orbiter::UpdateNativeAudio();
    }
    for (auto &plugin : m_Plugin) plugin.pModule->clbkVesselJump((OBJHANDLE)object);
    if (gclient) gclient->clbkVesselJump((OBJHANDLE)object);
}

void Orbiter::NotifyObjectSize(const Body *object)
{
    if (g_camera && object == g_camera->Target()) g_camera->Drag(Vector(0, 0, 0));
}

void Orbiter::SetWarpFactor(double warp, bool force, double delay)
{
    if (bPlayback && pConfig->CfgRecPlayPrm.bReplayWarp && !force) return;
    if (!std::isfinite(warp) || !std::isfinite(delay)) throw std::invalid_argument("Invalid time warp");
    const double old = td.Warp();
    td.SetWarp(std::clamp(warp, 0.1, 100000.0), delay);
    if (td.WarpChanged())
        for (auto &plugin : m_Plugin) plugin.pModule->clbkTimeAccChanged(td.Warp(), old);
}

void Orbiter::SetFOV(double fov, bool limit)
{
    if (g_camera->Aperture() == fov) return;
    g_camera->SetAperture(fov, limit);
    g_bForceUpdate = true;
}

void Orbiter::IncFOV(double delta) { SetFOV(g_camera->Aperture() + delta); }

bool Orbiter::Timejump(double mjd, int mode)
{
    if (!g_psys || !std::isfinite(mjd)) return false;
    tjump = {td.JumpTo(mjd), mode};
    g_psys->Timejump(tjump);
    if (g_camera->Target()) g_camera->Update();
    orbiter::UpdateNativeAudio();
    if (g_pane) g_pane->Timejump();
    for (auto &plugin : m_Plugin) plugin.pModule->clbkTimeJump(td.SimT0, tjump.dt, mjd);
    if (gclient) gclient->clbkTimeJump(td.SimT0, tjump.dt, mjd);
    return true;
}

void Orbiter::EndPlayback()
{
    for (auto *vessel : g_psys->GetVessels()) vessel->FRecorder_EndPlayback();
    FRecorder_ClosePlayback();
    bPlayback = false;
}

void Orbiter::ModulePreStep()
{
    for (auto &plugin : m_Plugin) plugin.pModule->clbkPreStep(td.SimT0, td.SimDT, td.MJD0);
    for (DWORD i = 0; i < g_psys->nVessel(); ++i)
        g_psys->GetVessel(i)->ModulePreStep(td.SimT0, td.SimDT, td.MJD0);
}

void Orbiter::ModulePostStep()
{
    for (DWORD i = 0; i < g_psys->nVessel(); ++i)
        g_psys->GetVessel(i)->ModulePostStep(td.SimT1, td.SimDT, td.MJD1);
    for (auto &plugin : m_Plugin) plugin.pModule->clbkPostStep(td.SimT1, td.SimDT, td.MJD1);
}

DLLEXPORT void oapiRegisterModule(oapi::Module *module) { g_pOrbiter->register_module = module; }
DLLEXPORT void oapiGetObjectName(OBJHANDLE object, char *name, int length)
{
    if (length > 0) std::strncpy(name, static_cast<Body *>(object)->Name(), length);
}

const char *Orbiter::KeyState() const { return simkstate; }
DLLEXPORT const char *oapiGetKeyState() { return g_pOrbiter->KeyState(); }

void Orbiter::NativeKeyEvent(unsigned int key, bool down)
{
    if (!key || key >= sizeof simkstate) return;
    simkstate[key] = down ? static_cast<char>(0x80) : 0;
    if (g_focusobj) {
        char keys[256];
        std::copy(std::begin(simkstate), std::end(simkstate), keys);
        if (g_focusobj->ConsumeBufferedKey(key, down, keys)) return;
    }
    if(down && KEYMOD_CONTROL(simkstate) && !KEYMOD_ALT(simkstate) && !KEYMOD_SHIFT(simkstate) &&
       g_camera && g_camera->IsInternal() && g_camera->GetIntMode()==CAMERA_VIRTUALCOCKPIT) {
        int direction=-1;
        if(key==OAPI_KEY_LEFT)direction=0;else if(key==OAPI_KEY_RIGHT)direction=1;
        else if(key==OAPI_KEY_UP)direction=2;else if(key==OAPI_KEY_DOWN)direction=3;
        if(direction>=0) {
            const auto &state=orbiter::NativeCockpitState();
            const int next=state.neighbours[direction];
            if(next>=0 && state.owner==reinterpret_cast<OBJHANDLE>(g_focusobj))LoadNativeVirtualCockpit(next);
        }
    }
}

void Orbiter::ReleaseNativeKeys()
{
    orbiter::platform::ReleaseJoystickControls();
    for (unsigned key = 1; key < sizeof simkstate; ++key)
        if (simkstate[key]) NativeKeyEvent(key, false);
}

void Orbiter::ResizeNativeViewport(int width, int height)
{
    if (gclient) gclient->clbkResizeNativeWindow(std::max(width,0), std::max(height,0));
    if (width <= 0 || height <= 0) return;
    viewW = width;
    viewH = height;
    viewBPP = 32;
    if (g_camera) g_camera->ResizeViewport(width, height);
}

void Orbiter::RenderNativeFrame() {
    orbiter::UpdateNativeAudio();
    if (g_camera && g_camera->IsInternal()) {
        orbiter::NativeMFDUpdateCockpit();
        orbiter::NativeCockpitState().Redraw(g_focusobj);
    }
    if (gclient) {
        gclient->clbkRenderScene();
        for (DWORD i=0; i<nsnote; ++i) snote[i]->Render();
        gclient->clbkImGuiNewFrame();
        auto &io=ImGui::GetIO();
        if(!io.BackendPlatformUserData) {io.DisplaySize=ImVec2(float(viewW),float(viewH));io.DisplayFramebufferScale=ImVec2(1,1);io.DeltaTime=1.0f/60.0f;}
        if(!nativeUiVisible_ && !orbiter::NativeMFDVisible() && !orbiter::NativeDialogsActive())ImGui::SetWindowFocus(nullptr);
        ImGui::NewFrame();
        if(nativeUiVisible_) {
            ImGui::SetNextWindowPos(ImVec2(16,16),ImGuiCond_FirstUseEver);
            const bool expanded=ImGui::Begin("Flight controls",&nativeUiVisible_,ImGuiWindowFlags_AlwaysAutoResize);
            if(expanded) {
                if(g_focusobj)ImGui::TextUnformatted(g_focusobj->Name());
                ImGui::TextUnformatted(IsRunning()?"Simulation running":"Simulation paused");
                if(ImGui::Button(IsRunning()?"Pause (F5)":"Resume (F5)"))TogglePause();
                ImGui::SameLine();if(ImGui::Button("Next spacecraft (F4)"))CycleNativeFocus();
                if(g_camera && g_focusobj && ImGui::Button(g_camera->IsInternal()?"Exterior view":"3D cockpit")) {
                    ReleaseNativeCockpitMouse();
                    if(g_camera->IsInternal()) {g_camera->Attach(g_focusobj,1);g_camera->SetDistance(3);g_camera->Update();}
                    else LoadNativeVirtualCockpit();
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Flight instruments: F2\nLook around: right-drag   Zoom: scroll\nChange seat: Ctrl + arrows\nCockpit flashlight: F\nLaptop / numpad controls: F6\nClose this panel: F1");
                if (ImGui::CollapsingHeader("Controllers")) {
                    const auto &devices = orbiter::platform::JoystickStates();
                    if (devices.empty()) ImGui::TextUnformatted("No controller connected");
                    ImGui::BeginChild("Controller devices", ImVec2(420, 180));
                    for (const auto &device : devices) {
                        ImGui::PushID(static_cast<int>(device.instance));
                        ImGui::TextUnformatted(device.name.c_str());
                        ImGui::Text("GUID: %s", device.guid.c_str());
                        if (!device.serial.empty()) ImGui::Text("Serial: %s", device.serial.c_str());
                        if (ImGui::SmallButton("Copy GUID")) ImGui::SetClipboardText(device.guid.c_str());
                        for (std::size_t i = 0; i < device.axes.size(); ++i)
                            ImGui::Text("Axis %u: %d", static_cast<unsigned>(i), int(device.axes[i]));
                        for (std::size_t i = 0; i < device.buttons.size(); ++i)
                            if (device.buttons[i]) ImGui::Text("Button %u pressed", static_cast<unsigned>(i));
                        for (std::size_t i = 0; i < device.hats.size(); ++i)
                            ImGui::Text("Hat %u: %u", static_cast<unsigned>(i), unsigned(device.hats[i]));
                        ImGui::Separator(); ImGui::PopID();
                    }
                    ImGui::EndChild();
                    ImGui::TextUnformatted("Recenter controls after leaving this panel.");
                }
            }
            ImGui::End();
        }
        orbiter::NativeMFDDraw();orbiter::NativeDialogsDraw();
        ImGui::Render();gclient->clbkImGuiRenderDrawData();gclient->clbkDisplayFrame();orbiter::NativeMFDFrameEnd();
    }
}

bool Orbiter::LoadNativeVirtualCockpit(int id) {
    if (id < -1 || !g_focusobj || !g_camera || !gclient) return false;
    g_camera->SetIntMode(CAMERA_VIRTUALCOCKPIT);
    g_camera->Attach(g_focusobj, 0);
    ReleaseNativeCockpitMouse();
    orbiter::NativeCockpitState().Reset(reinterpret_cast<OBJHANDLE>(g_focusobj));
    orbiter::NativeCockpitState().cockpitId=id<0?0:id;
    if (!g_focusobj->LoadVC(id) && !(id==-1 && g_focusobj->LoadVC(0))) {
        orbiter::NativeCockpitState().Reset();return false;
    }
    g_camera->ResetCockpitDir(false);
    g_camera->Update();
    return true;
}

void Orbiter::NativeCockpitMotion(double x,double y) {
    auto &state=orbiter::NativeCockpitState();state.mouseX=x;state.mouseY=y;
    if(!g_camera || !g_focusobj || state.owner!=reinterpret_cast<OBJHANDLE>(g_focusobj))return;
    Vector global;g_camera->ViewportToGlobalDir(int(x),int(y),global);
    const auto local=tmul(g_focusobj->GRot(),global);const auto eye=g_camera->CockpitPos();
    for(unsigned button=0;button<2;++button) {
        if(!state.capture[button])continue;
        const auto hit=state.Pick(_V(eye.x,eye.y,eye.z),_V(local.x,local.y,local.z),PANEL_MOUSE_DOWN|PANEL_MOUSE_PRESSED);
        if(hit && hit->id==state.capture[button]->id)state.capture[button]=hit;
    }
}
bool Orbiter::NativeCockpitMouse(unsigned button,bool down,double x,double y) {
    if (button>1) return false;
    auto &state=orbiter::NativeCockpitState();NativeCockpitMotion(x,y);
    if (!g_focusobj || state.owner!=reinterpret_cast<OBJHANDLE>(g_focusobj) || !g_camera || g_camera->IsExternal()) return false;
    const int pressed=button?PANEL_MOUSE_RBPRESSED:PANEL_MOUSE_LBPRESSED;
    const int event=down?(button?PANEL_MOUSE_RBDOWN:PANEL_MOUSE_LBDOWN):(button?PANEL_MOUSE_RBUP:PANEL_MOUSE_LBUP);
    if (down) {
        Vector global;g_camera->ViewportToGlobalDir(int(x),int(y),global);
        const auto local=tmul(g_focusobj->GRot(),global);const auto eye=g_camera->CockpitPos();
        state.capture[button]=state.Pick(_V(eye.x,eye.y,eye.z),_V(local.x,local.y,local.z),event|pressed);
        state.held[button]=state.capture[button].has_value();
    }
    const auto hit=state.capture[button];
    if (!hit) return false;
    const auto it=state.areas.find(hit->id);
    const int mask=it==state.areas.end()?0:it->second.mouse;
    if (!down) {state.capture[button].reset();state.held[button]=false;}
    if ((mask&event) && (!g_focusobj->bPlayback() || (mask&PANEL_MOUSE_ONREPLAY))) {
        Vector position(hit->position.x,hit->position.y,hit->position.z);
        if (g_focusobj->VCMouseEvent(hit->id,event,position)) {
            auto area=state.areas.find(hit->id);
            if(area!=state.areas.end() && (area->second.draw&PANEL_REDRAW_MOUSE))area->second.mouseRedraw=true;
        }
    }
    return true;
}
void Orbiter::ReleaseNativeCockpitMouse() {
    auto &state=orbiter::NativeCockpitState();
    for(unsigned button=0;button<2;++button) {
        if(state.capture[button]) NativeCockpitMouse(button,false,state.mouseX,state.mouseY);
        state.capture[button].reset();state.held[button]=false;
    }
}
void Orbiter::StepNativeCockpitMouse() {
    auto &state=orbiter::NativeCockpitState();
    if (!g_focusobj || state.owner!=reinterpret_cast<OBJHANDLE>(g_focusobj)) return;
    for(unsigned button=0;button<2;++button) {
        const auto hit=state.capture[button];if(!state.held[button] || !hit)continue;
        const auto it=state.areas.find(hit->id);if(it==state.areas.end())continue;
        const int event=button?PANEL_MOUSE_RBPRESSED:PANEL_MOUSE_LBPRESSED;
        if (!(it->second.mouse&event) || (g_focusobj->bPlayback() && !(it->second.mouse&PANEL_MOUSE_ONREPLAY)))continue;
        Vector position(hit->position.x,hit->position.y,hit->position.z);
        if (g_focusobj->VCMouseEvent(hit->id,event,position)) {
            auto area=state.areas.find(hit->id);
            if(area!=state.areas.end() && (area->second.draw&PANEL_REDRAW_MOUSE))area->second.mouseRedraw=true;
        }
    }
}
