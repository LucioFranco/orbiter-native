#include "Orbiter.h"
#include "Vessel.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==3,"CSM runtime and callback fixture required");
        const auto fixture=std::filesystem::absolute(argv[2]);
        std::filesystem::current_path(argv[1]);
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        Orbiter host;
        auto module=host.LoadModule(fixture.parent_path().string().c_str(),fixture.stem().string().c_str());
        require(module,"load pause callback observer");
        auto count=reinterpret_cast<int (*)()>(orbiter::platform::LibrarySymbol(module,"PauseCount"));
        auto paused=reinterpret_cast<bool (*)()>(orbiter::platform::LibrarySymbol(module,"LastPause"));
        require(count && paused,"pause observer exports");
        host.Launch("NasspCSM");
        oapiCameraAttach(oapiGetFocusObject(),0);
        orbiter::platform::SDLWindow window(host,true,true);
        auto key=[&](SDL_Scancode code,bool down,bool repeat=false) {
            SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
            e.key.scancode=code;e.key.down=down;e.key.repeat=repeat;
            require(SDL_PushEvent(&e) && window.Poll(),"dispatch pause test input");
        };
        auto toggle=[&]() {key(SDL_SCANCODE_F5,true);key(SDL_SCANCODE_F5,false);};
        constexpr double dt=1.0/60.0;
        for(int i=0;i<10;++i)host.StepSimulation(dt);
        auto vessel=oapiGetVesselInterface(oapiGetFocusObject());
        VECTOR3 position{},velocity{};vessel->GetGlobalPos(position);vessel->GetGlobalVel(velocity);
        const double sim=oapiGetSimTime(),system=oapiGetSysTime(),fuel=vessel->GetFuelMass();
        auto state=[&]() {std::ostringstream out;reinterpret_cast<Vessel *>(oapiGetFocusObject())->Write(out);return out.str();};
        const auto savedState=state();
        key(SDL_SCANCODE_K,true);toggle();
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"pause releases laptop flight input");
        host.StepSimulation(dt);
        require(oapiGetPause() && !host.IsRunning() && count()==1 && paused(),"pause applied once at tick boundary");
        key(SDL_SCANCODE_F5,true,true);
        for(int i=0;i<120;++i)host.StepSimulation(dt);
        VECTOR3 after{},afterVelocity{};vessel->GetGlobalPos(after);vessel->GetGlobalVel(afterVelocity);
        require(oapiGetSimTime()==sim && length(after-position)==0 && length(afterVelocity-velocity)==0 && vessel->GetFuelMass()==fuel,"paused CSM clock, orbit, velocity and fuel stay frozen");
        require(state()==savedState,"paused NASSP mission and AGC serialized state stay frozen");
        require(oapiGetSysTime()>system+2 && count()==1,"system clock advances while key repeat does not resume");
        require(window.Poll() && std::string(SDL_GetWindowTitle(window.Handle())).find("PAUSED")!=std::string::npos,"window title indicates pause");
        const double aperture=oapiCameraAperture();
        SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.y=1;
        require(SDL_PushEvent(&wheel) && window.Poll(),"paused zoom input");
        require(oapiCameraAperture()<aperture && oapiGetSimTime()==sim,"camera zoom works while paused");
        toggle();host.StepSimulation(dt);
        require(!oapiGetPause() && count()==2 && !paused(),"resume callback sent once");
        require(std::abs(oapiGetSimTime()-sim-dt)<1e-10,"resume advances one tick without paused-time catchup");
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"resume does not rearm held flight input");
        // Requests from SDK clients obey the same boundary and are idempotent.
        oapiSetPause(true);oapiSetPause(true);host.StepSimulation(dt);
        require(oapiGetPause() && count()==3,"SDK pause request and duplicate suppression");
        oapiSetPause(false);host.StepSimulation(dt);require(!oapiGetPause() && count()==4,"SDK resume");
        toggle();toggle();host.StepSimulation(dt);
        require(!oapiGetPause() && count()==4,"two toggles before a tick cancel");
        std::cout<<"Native pause, resume, clock isolation, input release, camera zoom and callbacks verified\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
