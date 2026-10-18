#pragma once
// Removable diagnostic transport: no NASSP or engine state reconstruction.
#include "Orbiter.h"
#include "Platform/SDLWindow.h"
#include "Platform/Joystick.h"
#include "Platform/DynamicLibrary.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

struct PDIWindowState {
    double sim=0,mission=0,height=0,yaw=0;
    uint64_t cycles=0;
    bool contact=false,detent=false,binding=false;
    std::array<double,18> motion{};
};

class PDIWindow {
    Orbiter &host;
    orbiter::platform::SDLWindow window;
    std::function<PDIWindowState()> observe;
    using Read=bool (*)(BYTE *,size_t,DWORD);
    Read read;
    NOTEHANDLE note=nullptr;
    std::ofstream log{"PDIWindow.jsonl",std::ios::trunc};
    std::atomic<bool> automated{true};
    std::atomic<unsigned> filtered{0};
    std::atomic<bool> ownedFocus{false};
    bool guarded=false,selfTest=false,fineChecked=false;
    unsigned frames=0;
    double handoffTime=0;
    int noteW=0,noteH=0;
    static void check(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
    static bool SDLCALL filter(void *context,SDL_Event *event) {
        auto &self=*static_cast<PDIWindow *>(context);
        const bool flight=event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP ||
            event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP ||
            event->type==SDL_EVENT_MOUSE_MOTION || event->type==SDL_EVENT_MOUSE_WHEEL ||
            event->type==SDL_EVENT_TEXT_INPUT || event->type==SDL_EVENT_WINDOW_FOCUS_GAINED ||
            event->type==SDL_EVENT_WINDOW_FOCUS_LOST;
        if(self.automated && event->type==SDL_EVENT_WINDOW_FOCUS_GAINED && self.ownedFocus)return true;
        if(self.automated && flight) {++self.filtered;return false;}
        if(!self.automated && event->type==SDL_EVENT_KEY_DOWN && event->key.scancode==SDL_SCANCODE_ESCAPE) {
            event->type=SDL_EVENT_QUIT;
        }
        return true;
    }
    void status(const std::string &text) {
        if(noteW!=host.ViewW() || noteH!=host.ViewH()) {
            if(note)oapiDelAnnotation(note);
            note=oapiCreateAnnotation(false,.72,_V(1,.9,.3));check(note,"cannot create diagnostic status");
            noteW=host.ViewW();noteH=host.ViewH();
        }
        oapiAnnotationSetPos(note,.015,.018,.985,.22);
        oapiAnnotationSetText(note,const_cast<char *>(text.c_str()));
        SDL_SetWindowTitle(window.Handle(),text.substr(0,text.find('\n')).c_str());
    }
    void record(const char *event) {
        auto s=observe();
        log<<std::setprecision(17)<<"{\"event\":\""<<event<<"\",\"sim_time\":"<<s.sim
            <<",\"mission_time\":"<<s.mission<<",\"cycles\":"<<s.cycles<<",\"height_ft\":"<<s.height
            <<",\"running\":"<<(host.IsRunning()?"true":"false")<<",\"virtual_binding_active\":"<<(s.binding?"true":"false")
            <<",\"aca_yaw\":"<<s.yaw<<",\"aca_out_of_detent\":"<<(s.detent?"true":"false")
            <<",\"joystick_devices\":"<<orbiter::platform::JoystickStates().size()
            <<",\"filtered_input_events\":"<<filtered.load()<<",\"rendered_frames\":"<<frames
            <<",\"continuation_guarded\":"<<(guarded?"true":"false")<<"}\n";
        log.flush();check(bool(log),"cannot write live-window observations");
    }
    void capture(const char *name) {
        const char *dir=std::getenv("ORBITER_PDI_CAPTURE_DIR");if(!dir)return;
        std::filesystem::create_directories(dir);
        std::vector<BYTE> pixels(size_t(host.ViewW())*host.ViewH()*4);
        check(read && read(pixels.data(),pixels.size(),host.ViewW()*4),"PDI framebuffer readback failed");
        std::ofstream out(std::filesystem::path(dir)/(std::string(name)+".ppm"),std::ios::binary);
        out<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
        for(size_t i=0;i<pixels.size();i+=4)out.write(reinterpret_cast<char *>(pixels.data()+i),3);
        check(bool(out),"cannot save PDI cockpit capture");
    }
    void key(SDL_Scancode code,bool down=true) {
        SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
        e.key.windowID=SDL_GetWindowID(window.Handle());e.key.scancode=code;e.key.down=down;
        check(SDL_PushEvent(&e)==1,"cannot inject SDL lifecycle key");
    }
    void focus(bool gain) {
        SDL_Event e{};e.type=gain?SDL_EVENT_WINDOW_FOCUS_GAINED:SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID=SDL_GetWindowID(window.Handle());
        check(SDL_PushEvent(&e)==(automated?0:1),"unexpected window focus event ownership");
    }
    bool tick(bool render=true) {
        if(!window.Poll())return false;
        auto s=observe();
        if(guarded || s.contact || s.height<=1200 || s.sim-handoffTime>=12) {guarded=true;host.Pause(true);}
        host.StepSimulation(1.0/60.0);
        status(guarded?"PDI cockpit: bounded continuation paused\nAltitude/contact or 12 s guard reached. Close window or Esc to exit.":
            host.IsRunning()?"PDI cockpit: manual controls enabled\nF5: pause. Laptop L/J: yaw right/left. F1: controls. Close window or Esc: exit.":
            "PDI cockpit: PAUSED - same live P64 host\nVirtual stick removed. F5: resume (up to 12 s / above 1200 ft). Laptop L/J: yaw. Close window or Esc: exit.");
        if(render){host.RenderNativeFrame();++frames;}
        return true;
    }
public:
    PDIWindow(Orbiter &h,HINSTANCE graphics,std::function<PDIWindowState()> observer,bool test):
        host(h),window(h,false,true),observe(std::move(observer)),
        read(reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(graphics,"orb_wgpu_read_output"))),selfTest(test) {
        check(bool(log) && read,"live-window log/readback unavailable");
        SDL_EventFilter previous=nullptr;void *data=nullptr;
        check(!SDL_GetEventFilter(&previous,&data),"diagnostic must not replace an existing SDL event filter");
        SDL_SetEventFilter(filter,this);
        // SDLWindow's normal background-input gate must not release the
        // isolated scripted stick. Only this owned focus event bypasses the
        // automation filter; OS focus changes stay blocked until handoff.
        ownedFocus=true;SDL_Event gain{};gain.type=SDL_EVENT_WINDOW_FOCUS_GAINED;
        gain.window.windowID=SDL_GetWindowID(window.Handle());
        const auto sent=SDL_PushEvent(&gain);ownedFocus=false;
        check(sent==1,"cannot assign diagnostic input ownership");
        check(SDL_SetWindowSize(window.Handle(),960,540),"cannot size diagnostic cockpit window");
    }
    ~PDIWindow() {SDL_SetEventFilter(nullptr,nullptr);if(note)oapiDelAnnotation(note);}
    void pump(unsigned ticks,bool fine) {
        if(ticks%300 && !fine)return;
        if(selfTest && fine && !fineChecked){fineChecked=true;isolationCheck();}
        check(window.Poll(),"window closed during automated PDI");
        // The isolated named virtual stick owns automation. Window focus and
        // ImGui capture must not interrupt its deterministic original input.
        orbiter::platform::SetJoystickInputEnabled(true);
        check(host.IsRunning(),"automation unexpectedly paused");
        status("Original Apollo 11: automated PDI / ACA (accelerated fixed steps)\nFlight input is locked until neutral-release handoff. Close window to cancel. Sim +"+std::to_string(ticks/60)+" s");
        host.RenderNativeFrame();++frames;
        if(ticks==0){record("automation_started");capture("original-pdi");}
        if(fine)SDL_Delay(16);
    }
    void isolationCheck() {
        const auto count=filtered.load();
        SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_F5;e.key.down=true;
        check(SDL_PushEvent(&e)==0,"automation must reject pause key");
        e.key.scancode=SDL_SCANCODE_L;check(SDL_PushEvent(&e)==0,"automation must reject flight key");
        e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.down=true;
        check(SDL_PushEvent(&e)==0,"automation must reject cockpit click");
        focus(false);focus(true);check(window.Poll(),"window closed during isolation check");
        orbiter::platform::SetJoystickInputEnabled(true);
        check(filtered>=count+5 && !KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD3),"automation input isolation failed");
        record("automation_input_isolation_checked");
    }
    void handoff(const std::function<void()> &release) {
        const auto before=observe();check(before.yaw==0 && !before.detent,"neutral ACA endpoint required");
        host.Pause(true);host.StepSimulation(1.0/60.0); // pause boundary: no advancing physics/AGC tick
        release();host.ReleaseNativeKeys();host.ReleaseNativeCockpitMouse();
        orbiter::platform::ReleaseJoystickControls();
        check(window.Poll(),"window closed at automation handoff"); // publish device loss while still locked
        auto after=observe();
        check(!host.IsRunning() && before.sim==after.sim && before.mission==after.mission &&
            before.cycles==after.cycles && before.motion==after.motion,"handoff must preserve same live clock and motion");
        check(!after.binding && after.yaw==0 && !after.detent,"virtual ACA ownership must be neutral and released");
        automated=false;handoffTime=after.sim;
        // Re-evaluate queued events now that controls are enabled. No held
        // automation input may become a manual command.
        SDL_SetEventFilter(filter,this);
        focus((SDL_GetWindowFlags(window.Handle()) & SDL_WINDOW_INPUT_FOCUS)!=0);
        check(tick(),"window closed at paused handoff");record("same_host_paused_handoff");capture("p64-paused-handoff");
        std::cout<<"Same live P64 host paused. Virtual stick removed. F5 resumes; laptop L/J yaw; close window or Esc exits. Continuation bounded to 12 s and above 1200 ft.\n"<<std::flush;
    }
    void run(bool selfTest) {
        if(selfTest) {
            auto step=[&](){check(tick(),"window closed during bounded lifecycle check");};
            auto before=observe();
            for(int i=0;i<12;++i)step();
            auto frozen=observe();check(before.sim==frozen.sim && before.cycles==frozen.cycles && before.motion==frozen.motion,
                "paused window must retain processor and flight state");record("pause_frozen_checked");
            focus(true);key(SDL_SCANCODE_F5);step();key(SDL_SCANCODE_F5,false);step();
            check(host.IsRunning() && observe().cycles>before.cycles,"F5 must unpause same live processor");record("unpause_checked");
            key(SDL_SCANCODE_L);for(int i=0;i<6;++i)step();
            check(observe().yaw!=0 && observe().detent && !observe().binding,"manual SDL keyboard must own original ACA after virtual detach");
            record("manual_keyboard_aca_checked");
            key(SDL_SCANCODE_L,false);for(int i=0;i<12;++i)step();
            check(observe().yaw==0 && !observe().detent,"manual keyboard release must neutralize original ACA");record("manual_neutral_checked");
            key(SDL_SCANCODE_L);step();focus(false);step();
            check(observe().yaw==0 && !observe().detent,"focus loss must release manual ACA");record("focus_release_checked");
            focus(true);key(SDL_SCANCODE_L,false);step();
            key(SDL_SCANCODE_F5);step();key(SDL_SCANCODE_F5,false);step();
            check(!host.IsRunning(),"F5 must pause manual continuation");record("manual_repause_checked");
            key(SDL_SCANCODE_F5);step();key(SDL_SCANCODE_F5,false);
            for(int i=0;i<721 && !guarded;++i)check(tick(i%60==0),"window closed during guard check");
            check(guarded && !host.IsRunning(),"bounded manual continuation must pause");record("continuation_guard_checked");
            before=observe();key(SDL_SCANCODE_F5);step();key(SDL_SCANCODE_F5,false);step();
            check(!host.IsRunning() && observe().sim==before.sim && observe().cycles==before.cycles,
                "guard must prevent additional continuation");record("guard_repause_checked");
            SDL_Event close{};close.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;close.window.windowID=SDL_GetWindowID(window.Handle());
            check(SDL_PushEvent(&close)==1 && !window.Poll(),"normal SDL window close must terminate cockpit loop");
            record("normal_close_checked");key(SDL_SCANCODE_ESCAPE);
            // A prior close can leave SDL's poll sentinel ahead of this synthetic
            // Esc. The real retained loop polls again on the next frame.
            check(!window.Poll() || !window.Poll(),"Escape must terminate cockpit loop");record("escape_close_checked");return;
        }
        while(tick())SDL_Delay(16);
        record("user_closed");
    }
};
