#include "Orbiter.h"
#include "Platform/SDLWindow.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==2,"CSM runtime required");
        std::filesystem::current_path(argv[1]);
        Orbiter host;host.Launch("NasspCSM");
        auto vessel=oapiGetVesselInterface(oapiGetFocusObject());
        auto step=[&](int count) {for(int i=0;i<count;++i)host.StepSimulation(1.0/60.0);};
        auto thrust=[&]() {
            VECTOR3 force{};
            for(DWORD i=0;i<vessel->GetThrusterCount();++i) {
                auto t=vessel->GetThrusterHandleByIndex(i);VECTOR3 direction{};
                vessel->GetThrusterDir(t,direction);
                force+=direction*(vessel->GetThrusterLevel(t)*vessel->GetThrusterMax(t));
            }
            return force;
        };
        auto idle=[&]() {
            for(DWORD i=0;i<vessel->GetThrusterCount();++i)
                if(vessel->GetThrusterLevel(vessel->GetThrusterHandleByIndex(i))>0)return false;
            return true;
        };
        step(120);vessel->SetAttitudeMode(RCS_LIN);
        require(idle(),"CSM jets initially idle");
        const struct {DWORD key;VECTOR3 direction;const char *name;} cases[]={
            {OAPI_KEY_NUMPAD2,_V(0,1,0),"up"},{OAPI_KEY_NUMPAD8,_V(0,-1,0),"down"},
            {OAPI_KEY_NUMPAD1,_V(-1,0,0),"left"},{OAPI_KEY_NUMPAD3,_V(1,0,0),"right"},
            {OAPI_KEY_NUMPAD6,_V(0,0,1),"forward"},{OAPI_KEY_NUMPAD9,_V(0,0,-1),"back"}
        };
        // Apollo's THC has a breakout threshold: Orbiter's 10% request must
        // not accidentally become a full jet firing through a default group.
        host.NativeKeyEvent(OAPI_KEY_LCONTROL,true);
        host.NativeKeyEvent(OAPI_KEY_NUMPAD2,true);step(10);
        require(idle(),"reduced request remains below Apollo THC breakout");
        host.ReleaseNativeKeys();step(3);
        for(const auto &command:cases) {
            const double fuel=vessel->GetTotalPropellantMass();
            host.NativeKeyEvent(command.key,true);step(60);
            VECTOR3 force=thrust();
            require(length(force)>100,"CSM RCS produces thrust");
            require(dotp(force,command.direction)/length(force)>0.95,"RCS thrust follows commanded translation axis");
            require(vessel->GetTotalPropellantMass()<fuel-0.01,"RCS firing consumes propellant");
            host.ReleaseNativeKeys();step(3);
            require(idle(),"CSM jets stop after key release");
            std::cout<<"CSM translation "<<command.name<<" verified ("<<length(force)<<" N)\n";
        }
        auto torque=[&]() {
            VECTOR3 moment{};
            for(DWORD i=0;i<vessel->GetThrusterCount();++i) {
                auto t=vessel->GetThrusterHandleByIndex(i);VECTOR3 direction{},position{};
                vessel->GetThrusterDir(t,direction);vessel->GetThrusterRef(t,position);
                // Orbiter uses left-handed vessel coordinates.
                moment+=crossp(direction*(vessel->GetThrusterLevel(t)*vessel->GetThrusterMax(t)),position);
            }
            return moment;
        };
        const struct {DWORD key;VECTOR3 axis;const char *name;} rotations[]={
            {OAPI_KEY_NUMPAD2,_V(1,0,0),"pitch up"},{OAPI_KEY_NUMPAD8,_V(-1,0,0),"pitch down"},
            {OAPI_KEY_NUMPAD1,_V(0,1,0),"yaw left"},{OAPI_KEY_NUMPAD3,_V(0,-1,0),"yaw right"},
            {OAPI_KEY_NUMPAD4,_V(0,0,-1),"bank left"},{OAPI_KEY_NUMPAD6,_V(0,0,1),"bank right"}
        };
        for(bool hardover:{false,true}) for(const auto &command:rotations) {
            vessel->SetAttitudeMode(RCS_ROT);
            VECTOR3 before{},after{};vessel->GetAngularVel(before);
            const double fuel=vessel->GetTotalPropellantMass();
            if(hardover)host.NativeKeyEvent(OAPI_KEY_LALT,true);
            host.NativeKeyEvent(command.key,true);
            VECTOR3 impulse{};
            for(int i=0;i<30;++i) {step(1);impulse+=torque()/60.0;}
            vessel->GetAngularVel(after);
            require(dotp(impulse,command.axis)>10,"RHC jets produce commanded angular impulse");
            require(dotp(after-before,command.axis)>1e-5,"Apollo jet torque changes angular velocity");
            require(vessel->GetTotalPropellantMass()<fuel-0.01,"rotation consumes propellant");
            host.ReleaseNativeKeys();vessel->SetAttitudeMode(RCS_LIN);step(3);
            // Rate damping may fire the opposing jets briefly after release.
            require(dotp(torque(),command.axis)<1,"mode change releases commanded rotation");
            step(120);require(idle(),"SCS finishes damping after rotation release");
            std::cout<<"CSM "<<(hardover?"hardover ":"normal ")<<command.name<<" verified\n";
        }
        for(DWORD modifier:{OAPI_KEY_LCONTROL,OAPI_KEY_LSHIFT}) {
            vessel->SetAttitudeMode(RCS_ROT);
            host.NativeKeyEvent(OAPI_KEY_LALT,true);
            host.NativeKeyEvent(OAPI_KEY_NUMPAD2,true);step(30);
            require(!idle(),"rotation active before shortcut modifier");
            host.NativeKeyEvent(OAPI_KEY_LALT,false);
            host.NativeKeyEvent(modifier,true);step(3);
            require(torque().x<1,"shortcut modifier releases previous RHC command");
            host.ReleaseNativeKeys();step(120);
            require(idle(),"rotation settles after modifier release");
        }
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        orbiter::platform::SDLWindow window(host,true,true);
        auto key=[&](SDL_Scancode code,bool down) {
            SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
            event.key.scancode=code;event.key.down=down;
            require(SDL_PushEvent(&event) && window.Poll(),"SDL laptop control dispatch");
        };
        vessel->SetAttitudeMode(RCS_ROT);
        key(SDL_SCANCODE_SLASH,true);key(SDL_SCANCODE_SLASH,false);step(1);
        require(vessel->GetAttitudeMode()==RCS_LIN,"laptop slash selects translation mode");
        const SDL_Scancode laptopKeys[]={SDL_SCANCODE_K,SDL_SCANCODE_8,SDL_SCANCODE_J,SDL_SCANCODE_L,SDL_SCANCODE_O,SDL_SCANCODE_9};
        for(unsigned i=0;i<6;++i) {
            key(laptopKeys[i],true);step(60);const auto force=thrust();
            require(length(force)>100 && dotp(force,cases[i].direction)/length(force)>0.95,"laptop key fires actual Apollo translation jets");
            key(laptopKeys[i],false);step(3);require(idle(),"laptop release stops translation jets");
        }
        key(SDL_SCANCODE_K,true);step(10);require(!idle(),"laptop thrust before focus loss");
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;
        require(SDL_PushEvent(&focus) && window.Poll(),"CSM focus loss");
        step(3);require(idle(),"focus loss stops actual Apollo jets");key(SDL_SCANCODE_K,false);
        key(SDL_SCANCODE_SLASH,true);key(SDL_SCANCODE_SLASH,false);step(1);
        require(vessel->GetAttitudeMode()==RCS_ROT,"laptop slash selects rotation mode");
        key(SDL_SCANCODE_LALT,true);key(SDL_SCANCODE_K,true);
        VECTOR3 impulse{};for(int i=0;i<30;++i) {step(1);impulse+=torque()/60.0;}
        require(impulse.x>10,"laptop Alt+K commands Apollo direct pitch jets");
        key(SDL_SCANCODE_F6,true);key(SDL_SCANCODE_F6,false);step(3);
        require(torque().x<1,"changing keyboard mode releases hardover rotation");
        step(120);require(idle(),"rotation damps after keyboard mode change");
        std::cout<<"Laptop controls verified through SDL and real Apollo RCS\n";
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
