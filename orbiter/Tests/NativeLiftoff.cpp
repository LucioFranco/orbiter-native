#include "Orbiter.h"
#include "NativeXPointer.h"
#include "Vessel.h"
#include "Camera.h"
#include "NativeCockpit.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <set>
#include <string>
#include <vector>
extern Camera *g_camera;
static void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
int main(int argc,char **argv) {
    try {
        require(argc==3 || argc==4,"graphics module and lunar surface runtime required");
        auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);
        Orbiter host;
        host.Cfg()->Load("Orbiter.cfg");
        require(!host.Cfg()->CfgLogicPrm.bStartPaused,"manual liftoff requires StartPaused=FALSE before Launch");
        auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());
        require(module && host.GetGraphicsClient()->clbkInitialise(),"initialize lunar surface renderer");
        const bool original=argc==4 && std::string(argv[3])=="--original";
        const bool elevated=argc==4 && !original;
        host.Launch(original?"Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m":"NasspLiftoff");
        std::set<std::string> initialVessels;
        for(DWORD i=0;i<oapiGetVesselCount();++i) {char name[256];oapiGetObjectName(oapiGetVesselByIndex(i),name,sizeof(name));initialVessels.insert(name);}
        if(original) require(initialVessels.size()==10,"retain all ten original mission vessels");auto sdk=oapiGetFocusInterface();auto vessel=reinterpret_cast<Vessel *>(oapiGetFocusObject());
        require(host.LoadNativeVirtualCockpit(),"load landed LM cockpit");orbiter::platform::SDLWindow window(host,false,true);
        auto step=[&](int n){for(int i=0;i<n;++i){const double before=oapiGetSimTime();host.StepSimulation(1.0/60.0);require(host.IsRunning()&&oapiGetSimTime()>before,"counted liftoff step must advance simulation");}};
        const double height=sdk->GetAltitude(ALTMODE_GROUND);
        if(elevated)require(std::abs(sdk->GetAltitude(ALTMODE_MEANRAD)-height-1234)<0.1,"landed LM uses the elevated terrain datum");
        require(sdk->GroundContact(),"scenario begins on lunar surface");
        step(120);
        require(sdk->GroundContact() && std::abs(sdk->GetAltitude(ALTMODE_GROUND)-height)<0.1 && sdk->GetGroundspeed()<0.05,"unpowered LM stays on the ground");
        if(!original) {
            // Clear the extended contact probes, then let the flight/contact solver land.
            VESSELSTATUS2 descending{};descending.version=2;sdk->GetStatusEx(&descending);
            sdk->GetGlobalOrientation(descending.arot);descending.vrot=_V(0,0,0);descending.status=0;
            const auto up=unit(descending.rpos);
            descending.rpos+=up*2.0;descending.rvel-=up*0.2;
            sdk->DefSetStateEx(&descending);step(1);
            std::cout<<"initial height="<<height<<" airborne altitude="<<sdk->GetAltitude(ALTMODE_GROUND)<<" contact="<<sdk->GroundContact()<<'\n';
            require(!sdk->GroundContact(),"LM begins touchdown check airborne");
            bool touched=false;double minimumHeight=sdk->GetAltitude(ALTMODE_GROUND);
            for(int i=0;i<900;++i) {
                step(1);touched=touched || sdk->GroundContact();
                minimumHeight=std::min(minimumHeight,sdk->GetAltitude(ALTMODE_GROUND));
            }
            std::cout<<"touchdown altitude="<<sdk->GetAltitude(ALTMODE_GROUND)
                     <<" minimum="<<minimumHeight<<" speed="<<sdk->GetGroundspeed()<<'\n';
            require(touched && sdk->GroundContact(),"descending LM acquires persistent ground contact");
            require(minimumHeight>height-0.5 && std::abs(sdk->GetAltitude(ALTMODE_GROUND)-height)<0.2,
                    "touchdown pads settle near the original ground-relative height");
            require(sdk->GetGroundspeed()<0.1,"touchdown settles before ascent preparation");
        }
        auto press=[&](int id,double v=0.2) {
            host.RenderNativeFrame();auto &areas=orbiter::NativeCockpitState();const auto &area=areas.areas.at(id);
            auto point=(area.points[0]+area.points[1])*(0.5*(1-v))+(area.points[2]+area.points[3])*(0.5*v);
            const auto eye=g_camera->CockpitPos();int x,y,ww,wh;
            require(g_camera->Direction2Viewport(mul(vessel->GRot(),Vector(point.x-eye.x,point.y-eye.y,point.z-eye.z)),x,y),"launch control visible");
            SDL_GetWindowSize(window.Handle(),&ww,&wh);SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.down=true;
            e.button.x=float(x)*ww/host.ViewW();e.button.y=float(y)*wh/host.ViewH();
            require(SDL_PushEvent(&e) && window.Poll(),"dispatch launch control click");
            require(areas.capture[0] && areas.capture[0]->id==id,"pick requested launch control");step(30);
            e.type=SDL_EVENT_MOUSE_BUTTON_UP;e.button.down=false;require(SDL_PushEvent(&e) && window.Poll(),"release launch control");step(30);
        };
        require(host.LoadNativeVirtualCockpit(4),"left explosive devices panel");
        press(3812);press(3818); // MASTER ARM, ascent helium pressurization
        require(host.LoadNativeVirtualCockpit(0),"commander engine arm panel");press(3112); // OFF -> ASC
        SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_RIGHTBRACKET;e.key.down=true;
        require(SDL_PushEvent(&e) && window.Poll(),"manual engine start");step(30);
        e.type=SDL_EVENT_KEY_UP;e.key.down=false;require(SDL_PushEvent(&e) && window.Poll(),"release engine start");
        step(1200);host.RenderNativeFrame();
        auto descent=oapiGetVesselByName("Eagle-DESCENTSTG");
        std::cout<<"altitude="<<sdk->GetAltitude(ALTMODE_GROUND)<<" contact="<<sdk->GroundContact()<<" vessels="<<oapiGetVesselCount()<<std::endl;
        require(descent && oapiGetVesselCount()==initialVessels.size()+1,"liftoff separates descent stage");
        require(!sdk->GroundContact() && sdk->GetAltitude(ALTMODE_GROUND)>height+20,"ascent stage climbs from lunar surface");
        if(elevated) {
            auto stage=oapiGetVesselInterface(descent);
            require(std::abs(stage->GetAltitude(ALTMODE_MEANRAD)-stage->GetAltitude(ALTMODE_GROUND)-1234)<0.1,"separated descent stage retains terrain-relative placement");
        }
        require(oapiGetVesselInterface(descent)->GroundContact(),"descent stage remains on surface");
        for(const auto &name:initialVessels)require(oapiGetVesselByName(const_cast<char *>(name.c_str()))!=nullptr,"retain original mission vessels through liftoff");
        using Read=bool (*)(BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));
        require(read,"framebuffer readback available");
        auto pixels=[&]() {
            host.RenderNativeFrame();std::vector<BYTE> image(size_t(host.ViewW())*host.ViewH()*4);
            require(read(image.data(),image.size(),host.ViewW()*4),"read lunar ascent cockpit");return image;
        };
        verifyNativeXPointer(host,vessel,read);
        const auto cockpit=pixels();
        // The Moon itself is bright: compare against a scene with Eagle hidden so
        // a blank cockpit cannot pass merely because lunar pixels are visible.
        std::vector<WORD> visibility;
        for(UINT i=0;i<sdk->GetMeshCount();++i) {
            visibility.push_back(sdk->GetMeshVisibilityMode(i));sdk->SetMeshVisibilityMode(i,MESHVIS_NEVER);
        }
        const auto background=pixels();size_t changed=0;
        for(size_t i=0;i<cockpit.size();i+=4)
            if(!std::equal(cockpit.begin()+i,cockpit.begin()+i+3,background.begin()+i))++changed;
        for(UINT i=0;i<visibility.size();++i)sdk->SetMeshVisibilityMode(i,visibility[i]);
        require(changed>10000,"ascent cockpit remains visible above lunar scene after staging");
        require(pixels()==cockpit,"restoring mesh visibility preserves ascent cockpit rendering");
        if(const char *path=std::getenv("ORBITER_LIFTOFF_CAPTURE")) {
            std::ofstream image(path,std::ios::binary);image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<cockpit.size();i+=4)image.write(reinterpret_cast<const char *>(cockpit.data()+i),3);
            require(bool(image),"save requested lunar ascent capture");
        }
        require(host.SaveScenario("NativeLunarAscent","Native lunar surface launch",0),"save lunar ascent");
        std::cout<<"Native lunar surface stability, cockpit-commanded staging and liftoff verified\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
