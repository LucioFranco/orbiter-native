// Native presentation/ownership for Orbiter's original Instrument classes.
#include "NativeMFD.h"
#include "NativeCockpit.h"
#include "Orbiter.h"
#include "Mfd.h"
#include "MfdOrbit.h"
#include "Pane.h"
#include "Select.h"
#include "imgui.h"
#include <array>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
extern Orbiter *g_pOrbiter;
extern Vessel *g_focusobj;
HELPCONTEXT DefHelpContext{};
namespace {
struct Slot {std::unique_ptr<Instrument> instrument;int previous=MFD_ORBIT;};
std::unordered_map<Vessel *,std::array<Slot,MAXMFD>> displays;
bool visible=false,resources=false,frameActive=false;
std::vector<SURFHANDLE> frameSurfaces;
void retainFrameSurface(Instrument *instrument) {
    if(frameActive && instrument && instrument->Surface()) {
        auto surface=instrument->Surface();g_pOrbiter->GetGraphicsClient()->clbkIncrSurfaceRef(surface);frameSurfaces.push_back(surface);
    }
}
Instrument::Spec spec() {return {512,512,6,6,40,78,0};}
void initResources() {
    if(!resources) {Instrument::GlobalInit(g_pOrbiter->GetGraphicsClient());resources=true;}
}
void closeMode(int mode) {
    orbiter::NativeDialogsReset();
    for(auto &[vessel,slots]:displays)for(auto &slot:slots)
        if(slot.instrument && slot.instrument->Type()==mode) {retainFrameSurface(slot.instrument.get());slot.instrument.reset();slot.previous=MFD_ORBIT;}
}
}
namespace orbiter {
void NativeMFDInit() {
    Instrument::RegisterBuiltinModes();
    // These modes have not yet joined the native build. Keep their SDK IDs but
    // omit them from the original instrument's mode-selection pages.
    for(int mode:{MFD_MAP,MFD_HSI,MFD_LANDING,MFD_DOCKING,MFD_OPLANEALIGN,MFD_OSYNC,MFD_TRANSFER,MFD_COMMS})Instrument::DisableMode(mode);
}
void NativeMFDShutdown() {
    displays.clear();NativeMFDFrameEnd();Instrument_Orbit::ResetSavedState();visible=false;
    if(resources)Instrument::GlobalExit(g_pOrbiter->GetGraphicsClient());resources=false;
}
void NativeMFDReleaseRegistry() {
    while(Instrument::nGlobalModes>BUILTIN_MFD_MODES)
        Instrument::UnregisterUserMode(Instrument::GlobalMode[Instrument::nGlobalModes-1].id);
    delete []Instrument::GlobalMode;Instrument::GlobalMode=nullptr;Instrument::nGlobalModes=0;
    Instrument::ClearDisabledModes();Instrument::nextmodeid=BUILTIN_MFD_MODES+1;
}
void NativeMFDFrameEnd() {
    for(auto surface:frameSurfaces)g_pOrbiter->GetGraphicsClient()->clbkReleaseSurface(surface);
    frameSurfaces.clear();frameActive=false;
}
void NativeMFDForget(Vessel *vessel) {
    NativeDialogsReset();
    if(auto it=displays.find(vessel);it!=displays.end())for(auto &slot:it->second)retainFrameSurface(slot.instrument.get());
    displays.erase(vessel);Instrument_Orbit::ResetSavedState();
    for(auto &[owner,slots]:displays)for(auto &slot:slots)if(slot.instrument)slot.instrument->ProcessMessage(MSG_KILLVESSEL,vessel);
}
Instrument *NativeMFDGet(int id) {
    if(id<0 || id>=MAXMFD || !g_focusobj)return nullptr;
    auto it=displays.find(g_focusobj);return it==displays.end()?nullptr:it->second[id].instrument.get();
}
bool NativeMFDOpen(int id,int mode) {
    if(id<0 || id>=MAXMFD || !g_focusobj || !g_pOrbiter->GetGraphicsClient())return false;
    auto &slot=displays[g_focusobj][id];
    if(slot.instrument && slot.instrument->Type()==mode)return false;
    std::unique_ptr<Instrument> candidate;
    if(mode!=MFD_NONE) {
        initResources();candidate.reset(Instrument::Create(mode,nullptr,id,spec(),g_focusobj));
        if(!candidate)return false;
    }
    NativeDialogsReset();
    if(slot.instrument)slot.previous=slot.instrument->Type();
    retainFrameSurface(slot.instrument.get());slot.instrument=std::move(candidate);
    if(slot.instrument)slot.instrument->Update(1.0);
    g_focusobj->MFDchanged(id,mode);return true;
}
void NativeMFDPower(int id) {
    if(id<0 || id>=MAXMFD || !g_focusobj)return;
    auto &slot=displays[g_focusobj][id];
    NativeMFDOpen(id,slot.instrument?MFD_NONE:slot.previous);
}
void NativeMFDToggle() {
    visible=!visible;
    if(!visible)NativeDialogsReset();
    if(visible && g_focusobj && !displays.count(g_focusobj)) {
        NativeMFDOpen(0,MFD_ORBIT);
        NativeMFDOpen(1,MFD_SURFACE);
    }
}
bool NativeMFDVisible() {return visible;}
void NativeMFDUpdateCockpit() {
    const auto &cockpit=NativeCockpitState();
    if(cockpit.owner!=reinterpret_cast<OBJHANDLE>(g_focusobj))return;
    for(const auto &[id,spec]:cockpit.mfdScreens)
        if(auto instrument=NativeMFDGet(id))instrument->Update(1.0);
}
void NativeMFDDraw() {
    if(!visible || !g_focusobj)return;
    frameActive=true;
    std::vector<int> ids{0,1};
    const auto &cockpit=NativeCockpitState();
    if(cockpit.owner==reinterpret_cast<OBJHANDLE>(g_focusobj))
        for(const auto &[id,spec]:cockpit.mfdScreens)if(id>=2 && id<MAXMFD)ids.push_back(id);
    ImGui::SetNextWindowPos(ImVec2(16,180),ImGuiCond_FirstUseEver);
    if(ids.size()>2) {
        const auto size=ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSizeConstraints(ImVec2(0,0),ImVec2(std::max(350.f,size.x-32),std::max(200.f,size.y-196)));
    }
    if(ImGui::Begin("Flight instruments (F2)",&visible,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(g_focusobj->Name());
        for(size_t position=0;position<ids.size();++position) {
            const int id=ids[position];ImGui::PushID(id);ImGui::BeginGroup();
            if(id<2)ImGui::TextUnformatted(id==0?"Left MFD":"Right MFD");
            else ImGui::Text("Display %d",id+1);
            auto instrument=NativeMFDGet(id);
            const char *current=instrument?(instrument->Type()==MFD_ORBIT?"Orbit":instrument->Type()==MFD_SURFACE?"Surface":"Addon") : "Off";
            if(instrument)for(DWORD i=0;i<Instrument::nGlobalModes;++i)
                if(Instrument::GlobalMode[i].id==instrument->Type())current=Instrument::GlobalMode[i].spec->name;
            if(ImGui::BeginCombo("##mode",current)) {
                if(ImGui::Selectable("Off",!instrument)) {NativeMFDOpen(id,MFD_NONE);instrument=nullptr;}
                for(DWORD i=0;i<Instrument::nGlobalModes;++i) {
                    const auto &mode=Instrument::GlobalMode[i];if(Instrument::IsDisabledMode(mode.id))continue;
                    if(ImGui::Selectable(mode.spec->name,instrument && instrument->Type()==mode.id)) {
                        NativeMFDOpen(id,mode.id);instrument=NativeMFDGet(id);
                    }
                }
                ImGui::EndCombo();
            }
            instrument=NativeMFDGet(id);
            if(instrument) {
                instrument->Update(1.0);
                auto gc=g_pOrbiter->GetGraphicsClient();
                ImGui::Image(ImTextureRef(gc->clbkImGuiSurfaceTexture(instrument->Surface())),ImVec2(320,320));
                for(int button=0;button<12;++button) {
                    if(button%4)ImGui::SameLine();ImGui::PushID(button);
                    instrument=NativeMFDGet(id);const char *label=instrument?instrument->ButtonLabel(button):nullptr;
                    if(label) {
                        // Copy-to-HUD remains unavailable until the HUD port.
                        ImGui::BeginDisabled(std::string(label)=="HUD");
                        if(ImGui::Button(label,ImVec2(74,0)))oapiProcessMFDButton(id,button,PANEL_MOUSE_LBDOWN);
                        ImGui::EndDisabled();
                    }else ImGui::Dummy(ImVec2(74,ImGui::GetFrameHeight()));
                    ImGui::PopID();
                }
            }else ImGui::Dummy(ImVec2(320,320));
            ImGui::EndGroup();ImGui::PopID();if(position%2==0 && position+1<ids.size())ImGui::SameLine();
        }
    }
    ImGui::End();
}
void NativeMFDRead(const std::string &path) {
    if(!g_pOrbiter->GetGraphicsClient() || !g_focusobj)return;
    for(int id=0;id<MAXMFD;++id) {
        std::ifstream input(path);std::string line;
        const std::string header="BEGIN_MFD "+(id==0?std::string("Left"):id==1?std::string("Right"):std::to_string(id+1));
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r')line.pop_back();
            if(line!=header)continue;
            initResources();auto instrument=std::unique_ptr<Instrument>(Instrument::Create(input,nullptr,id,spec(),g_focusobj));
            if(instrument)displays[g_focusobj][id].instrument=std::move(instrument);
            break;
        }
    }
}
void NativeMFDWrite(std::ostream &out) {
    for(int id=0;id<MAXMFD;++id)if(auto instrument=NativeMFDGet(id))instrument->Write(out);
}
}
DLLEXPORT void oapiOpenMFD(int mode,int id) {orbiter::NativeMFDOpen(id,mode);}
DLLEXPORT int oapiRegisterMFDMode(MFDMODESPEC &spec) {return Instrument::RegisterUserMode(&spec);}
DLLEXPORT int oapiRegisterMFDMode(MFDMODESPECEX &spec) {return Instrument::RegisterUserMode(&spec);}
DLLEXPORT bool oapiUnregisterMFDMode(int mode) {closeMode(mode);return Instrument::UnregisterUserMode(mode);}
DLLEXPORT void oapiDisableMFDMode(int mode) {closeMode(mode);Instrument::DisableMode(mode);}
DLLEXPORT int oapiGetMFDModeSpecEx(char *name,MFDMODESPECEX **spec) {return Instrument::ModeFromName(name,spec);}
DLLEXPORT int oapiGetMFDModeSpec(char *name,MFDMODESPEC **spec) {return Instrument::ModeFromNameOld(name,spec);}
