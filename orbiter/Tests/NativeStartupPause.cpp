// Exercise the existing StartPaused setting through the real native host/SDL.
#include "Orbiter.h"
#include "Vessel.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
extern TimeData td;
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static std::string agc(const std::string& state){
    auto begin=state.find("AGC_BEGIN"),end=state.find("AGC_END",begin);
    require(begin!=std::string::npos && end!=std::string::npos,"real CSM AGC state required");
    return state.substr(begin,end-begin);
}
int main(int argc,char **argv){try{
    require(argc==4||argc==5,"runtime, callback fixture, mode, optional graphics required");
    const auto fixture=std::filesystem::absolute(argv[2]);const std::string mode=argv[3];
    const bool startPaused=mode.rfind("true-",0)==0,graphics=argc==5;
    const double initial=mode=="true-fixed"?1.0/60.0:mode=="true-command-fixed"?0.02:0.01;
    constexpr double dt=1.0/60.0;
    std::filesystem::current_path(argv[1]);if(!graphics)SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    Orbiter host;
    if(mode=="true-command-fixed")host.Cfg()->CfgCmdlinePrm.FixedStep=0.02;
    if(graphics){const auto path=std::filesystem::absolute(argv[4]);require(host.LoadModule(path.parent_path().string().c_str(),path.stem().string().c_str())&&host.GetGraphicsClient()->clbkInitialise(),"startup GPU renderer");}
    auto module=host.LoadModule(fixture.parent_path().string().c_str(),fixture.stem().string().c_str());require(module,"startup callback observer");
    auto symbol=[&](const char *name){auto p=orbiter::platform::LibrarySymbol(module,name);require(p,name);return p;};
    auto count=reinterpret_cast<int(*)()>(symbol("PauseCount"));
    auto starts=reinterpret_cast<int(*)()>(symbol("StartCount"));
    auto pre=reinterpret_cast<int(*)()>(symbol("PreCount"));
    auto post=reinterpret_cast<int(*)()>(symbol("PostCount"));
    auto order=reinterpret_cast<const char*(*)()>(symbol("CallbackOrder"));
    host.Launch("NasspCSM");
    require(host.Cfg()->CfgLogicPrm.bStartPaused==startPaused,"configured startup policy");
    require(starts()==1 && count()==0,"start callback before deferred pause boundary");
    const double loaded=oapiGetSimTime();
    require(pre()==(startPaused?1:0)&&post()==(startPaused?1:0),"StartPaused must execute exactly one initial world step; running startup none");
    require(loaded==(startPaused?initial:0),"stock startup interval/fixed-step priority");
    require(std::string(order())==(startPaused?"start;pre;post;":"start;"),"initial callback order");
    if(graphics)require(host.LoadNativeVirtualCockpit(),"startup CSM cockpit");
    oapiCameraAttach(oapiGetFocusObject(),0);
    orbiter::platform::SDLWindow window(host,!graphics,true);
    auto key=[&](SDL_Scancode code,bool down){SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.scancode=code;e.key.down=down;require(SDL_PushEvent(&e)&&window.Poll(),"startup SDL input");};
    auto toggle=[&](){key(SDL_SCANCODE_F5,true);key(SDL_SCANCODE_F5,false);};
    auto state=[&](){std::ostringstream out;reinterpret_cast<Vessel*>(oapiGetFocusObject())->Write(out);return out.str();};
    if(startPaused){key(SDL_SCANCODE_K,true);host.StepSimulation(dt);require(oapiGetPause()&&!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2)&&count()==1,"pending startup pause applies once and releases input");}
    else{host.StepSimulation(dt);require(!oapiGetPause()&&oapiGetSimTime()==dt&&pre()==1&&post()==1,"false/default first caller step unchanged");toggle();host.StepSimulation(dt);require(oapiGetPause()&&count()==1,"normal F5 pause");}
    require(window.Poll()&&std::string(SDL_GetWindowTitle(window.Handle())).find("PAUSED")!=std::string::npos,"visible paused title");
    auto sdk=oapiGetFocusInterface();VECTOR3 position{},velocity{};sdk->GetGlobalPos(position);sdk->GetGlobalVel(velocity);
    const double pausedTime=oapiGetSimTime(),pausedMjd=oapiGetSimMJD(),fuel=sdk->GetFuelMass(),system=oapiGetSysTime();
    const auto saved=state();const int beforePre=pre(),beforePost=post();
    // 600 virtual idle seconds bracket the measured 576.7-second acquisition.
    const unsigned idle=graphics?120:36000;
    for(unsigned i=0;i<idle;++i){host.StepSimulation(dt);if(graphics||i%60==0)require(window.Poll(),"idle window stays open");host.RenderNativeFrame();}
    VECTOR3 after{},afterVelocity{};sdk->GetGlobalPos(after);sdk->GetGlobalVel(afterVelocity);
    require(oapiGetSimTime()==pausedTime&&oapiGetSimMJD()==pausedMjd&&length(after-position)==0&&length(afterVelocity-velocity)==0&&sdk->GetFuelMass()==fuel,"idle frames freeze clock, MJD, trajectory and fuel");
    const auto afterState=state();require(agc(afterState)==agc(saved),"paused AGC frozen across idle/render frames");
    if(!graphics)require(afterState==saved,"paused complete NASSP serialized state frozen");
    require(pre()==beforePre&&post()==beforePost&&count()==1&&oapiGetSysTime()>system,"paused frames advance only system time; no world callbacks");
    toggle();host.StepSimulation(dt);require(!oapiGetPause()&&count()==2&&oapiGetSimTime()==pausedTime+dt,"F5 resume advances one normal step without catchup");
    key(SDL_SCANCODE_L,true);host.StepSimulation(dt);require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD3),"resumed laptop flight input");
    key(SDL_SCANCODE_L,false);require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD3),"resumed input release");
    const double closedTime=oapiGetSimTime();SDL_Event close{};close.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;require(SDL_PushEvent(&close)&&!window.Poll(),"normal SDL window close path");require(oapiGetSimTime()==closedTime,"close does not advance simulation");
    require(host.SaveScenario("StartupPauseClosed","Startup pause regression; original host controls",0),"normal save after close");
    std::ofstream result("startup-pause-result.json");result<<"{\"mode\":\""<<mode<<"\",\"graphics\":"<<(graphics?"true":"false")<<",\"startup_sim_time_s\":"<<loaded<<",\"idle_frames\":"<<idle<<",\"paused_sim_time_s\":"<<pausedTime<<",\"final_sim_time_s\":"<<closedTime<<",\"startup_world_steps\":"<<(startPaused?1:0)<<",\"pause_callbacks\":"<<count()<<",\"passed\":true}\n";
    std::cout<<"Verified StartPaused mode="<<mode<<" startup="<<loaded<<" idle_frames="<<idle<<" resume/input/close\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
