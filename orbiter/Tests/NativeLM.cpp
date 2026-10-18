#include "Orbiter.h"
#include "NativeXPointer.h"
#include "Vessel.h"
#include "Camera.h"
#include "NativeCockpit.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstdlib>
extern Camera *g_camera;
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==3,"graphics plugin and LM runtime required");
        const auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);
        Orbiter host;auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());
        require(module && host.GetGraphicsClient()->clbkInitialise(),"initialize graphics");
        host.Launch("NasspLM");require(host.LoadNativeVirtualCockpit(),"load Eagle cockpit");
        orbiter::platform::SDLWindow window(host);
        auto vessel=reinterpret_cast<Vessel *>(oapiGetFocusObject());
        auto &areas=orbiter::NativeCockpitState();
        using Read=bool (*)(BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));require(read,"framebuffer readback");
        auto step=[&](int n) {for(int i=0;i<n;++i)host.StepSimulation(1.0/60.0);};
        auto display=[&]() {std::ostringstream out;vessel->Write(out);auto state=out.str();auto a=state.find("DSKY_BEGIN"),b=state.find("DSKY_END",a);require(a!=std::string::npos && b!=std::string::npos,"LM saves DSKY state");return state.substr(a,b-a);};
        step(30);host.RenderNativeFrame();
        std::vector<BYTE> pixels(size_t(host.ViewW())*host.ViewH()*4);
        require(read(pixels.data(),pixels.size(),host.ViewW()*4),"read rendered LM");
        size_t visible=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]+pixels[i+1]+pixels[i+2]>30)++visible;
        require(visible>10000,"real LM cockpit geometry is visible");
        if(const auto path=std::getenv("ORBITER_LM_CAPTURE")) {
            std::ofstream image(path,std::ios::binary);image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<pixels.size();i+=4)image.write(reinterpret_cast<const char *>(pixels.data()+i),3);
            require(bool(image),"save requested LM capture");
        }
        verifyNativeXPointer(host,vessel,read);
        // NASSP defines twelve LM viewpoints, including ECS, AOT and both hatches.
        for(int view=0;view<12;++view) {
            require(host.LoadNativeVirtualCockpit(view) && areas.cockpitId==view,"select LM viewpoint");
            host.RenderNativeFrame();require(!areas.areas.empty(),"LM viewpoint registers controls");
        }
        require(host.LoadNativeVirtualCockpit(3),"select DSKY viewpoint");host.RenderNativeFrame();
        const auto &dsky=areas.areas.at(3425);
        require(dsky.surface && dsky.target && dsky.redraws>0,"LM DSKY dynamic texture active");
        using ReadSurface=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
        auto readSurface=reinterpret_cast<ReadSurface>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));
        const auto width=dsky.rect.right-dsky.rect.left,height=dsky.rect.bottom-dsky.rect.top;
        std::vector<BYTE> initial(size_t(width)*height*4),lamp(initial.size());
        require(readSurface && readSurface(dsky.surface,initial.data(),initial.size(),width*4),"read initial LM DSKY texture");
        auto press=[&](int id,double v=0.5,int button=SDL_BUTTON_LEFT) {
            const auto &key=areas.areas.at(id);auto point=key.points[0];
            if(key.shape==2) {point=(key.points[0]+key.points[1])*(0.5*(1-v))+(key.points[2]+key.points[3])*(0.5*v);}
            const auto eye=g_camera->CockpitPos();const Vector ray(point.x-eye.x,point.y-eye.y,point.z-eye.z);
            int x,y,ww,wh;require(g_camera->Direction2Viewport(mul(vessel->GRot(),ray),x,y),"LM DSKY key visible");
            SDL_GetWindowSize(window.Handle(),&ww,&wh);
            SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=button;e.button.down=true;
            e.button.x=float(x)*ww/host.ViewW();e.button.y=float(y)*wh/host.ViewH();
            require(SDL_PushEvent(&e) && window.Poll(),"LM key down");
            const int slot=button==SDL_BUTTON_RIGHT?1:0;
            require(areas.capture[slot] && areas.capture[slot]->id==id,"LM ray picking selects requested key");
            step(30);e.type=SDL_EVENT_MOUSE_BUTTON_UP;e.button.down=false;
            require(SDL_PushEvent(&e) && window.Poll(),"LM key up");step(30);
        };
        press(3401);press(3408);press(3410); // VERB 35
        std::cout<<display();require(display().find("VERB 35")!=std::string::npos,"LGC echoes clicked VERB 35");
        press(3418); // ENTER
        auto result=display();std::cout<<result;
        require(result.find("PROG 88")!=std::string::npos && result.find("VERB 88")!=std::string::npos && result.find("NOUN 88")!=std::string::npos,"LGC executes display test");
        host.RenderNativeFrame();
        require(readSurface(dsky.surface,lamp.data(),lamp.size(),width*4) && lamp!=initial,"LGC display test updates the cockpit texture");
        require(host.SaveScenario("NativeLMSaved","Native LM cockpit test",0),"save LM state");
        require(host.LoadNativeVirtualCockpit(0),"commander propulsion controls");host.RenderNativeFrame();
        auto sdk=oapiGetFocusInterface();
        auto state=[&]() {std::ostringstream out;vessel->Write(out);return out.str();};
        auto key=[&](SDL_Scancode code,bool down) {
            SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.scancode=code;e.key.down=down;
            require(SDL_PushEvent(&e) && window.Poll(),"LM propulsion key dispatch");
        };
        press(3112,0.8); // ENG ARM: DES
        require(state().find("EngineArmSwitch 0 0")!=std::string::npos,"arm descent engine through cockpit");
        press(3110,0.8); // THR CONT: MAN
        require(state().find("THRContSwitch 0 0")!=std::string::npos,"manual throttle through cockpit");
        THRUSTER_HANDLE engine=nullptr;
        for(DWORD i=0;i<sdk->GetThrusterCount();++i) {
            auto t=sdk->GetThrusterHandleByIndex(i);
            if(sdk->GetThrusterMax(t)>40000) {require(!engine,"one descent engine");engine=t;}
        }
        require(engine && sdk->GetThrusterLevel(engine)==0,"descent engine initially off");
        window.SetLaptopControls(true);
        const double fuel=sdk->GetTotalPropellantMass();
        key(SDL_SCANCODE_RIGHTBRACKET,true);step(30);key(SDL_SCANCODE_RIGHTBRACKET,false);step(120);
        std::cout<<"DPS ignition level "<<sdk->GetThrusterLevel(engine)<<'\n';
        require(sdk->GetThrusterLevel(engine)>0.05,"manual start ignites descent engine");
        const double low=sdk->GetThrusterLevel(engine);
        key(SDL_SCANCODE_M,true);step(180);key(SDL_SCANCODE_M,false);step(60);
        std::cout<<"DPS throttled level "<<sdk->GetThrusterLevel(engine)<<'\n';
        require(sdk->GetThrusterLevel(engine)>low+0.2,"TTCA increases actual descent thrust");
        require(sdk->GetTotalPropellantMass()<fuel-1.0,"descent burn consumes propellant");
        using ShowExhaust=void (*)(bool);
        auto showExhaust=reinterpret_cast<ShowExhaust>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_exhaust_visible"));
        require(showExhaust,"exhaust visibility diagnostic");
        g_camera->Attach(vessel,1);g_camera->SetTrackMode(CAMERA_TARGETRELATIVE);
        g_camera->SetDistance(2);g_camera->AddPhi(PI-2*g_camera->Phi());
        g_camera->AddTheta(-2*g_camera->Theta());g_camera->Update();
        auto exhaustImage=[&]() {
            host.RenderNativeFrame();std::vector<BYTE> image(size_t(host.ViewW())*host.ViewH()*4);
            require(read(image.data(),image.size(),host.ViewW()*4),"read LM exhaust scene");return image;
        };
        const auto burning=exhaustImage();showExhaust(false);const auto noFlame=exhaustImage();
        for(DWORD i=0;i<sdk->GetExhaustCount();++i)if(sdk->GetExhaustLevel(i)>0) {
            EXHAUSTSPEC spec{};sdk->GetExhaustSpec(i,&spec);
            std::cout<<"Active exhaust "<<i<<" level="<<sdk->GetExhaustLevel(i)<<" length="<<spec.lsize<<" texture="<<(spec.tex!=nullptr)<<'\n';
        }
        size_t flamePixels=0;
        for(size_t i=0;i<burning.size();i+=4)
            if(!std::equal(burning.begin()+i,burning.begin()+i+3,noFlame.begin()+i))++flamePixels;
        std::cout<<"Visible LM exhaust pixels="<<flamePixels<<'\n';
        showExhaust(true);require(exhaustImage()==burning,"restoring exhaust reproduces the exact burning frame");
        if(const char *path=std::getenv("ORBITER_EXHAUST_CAPTURE")) {
            std::ofstream image(path,std::ios::binary);image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<burning.size();i+=4)image.write(reinterpret_cast<const char *>(burning.data()+i),3);
            require(bool(image),"save LM exhaust capture");
        }
        require(flamePixels>100,"original LM DPS exhaust definition produces a visible flame");
        require(host.LoadNativeVirtualCockpit(0),"restore cockpit after exterior exhaust check");
        // Exercise the real LM dust source near the Moon, then restore the orbit.
        using ParticleStats=bool (*)(size_t *,size_t *);
        auto particleStats=reinterpret_cast<ParticleStats>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_particle_stats"));
        require(particleStats,"particle diagnostics available");
        VESSELSTATUS2 orbit{};orbit.version=2;sdk->GetStatusEx(&orbit);auto lowPass=orbit;
        const double groundHeight=sdk->GetAltitude(ALTMODE_MEANRAD)-sdk->GetAltitude(ALTMODE_GROUND);
        lowPass.rpos=unit(orbit.rpos)*(oapiGetSize(orbit.rbody)+groundHeight+10.0);lowPass.rvel=_V(0,0,0);
        sdk->DefSetStateEx(&lowPass);step(30);
        unsigned dustSources=0;
        for(DWORD i=0;i<sdk->GetThrusterCount();++i) {
            auto source=sdk->GetThrusterHandleByIndex(i);
            if(sdk->GetThrusterMax(source)!=0 || sdk->GetThrusterLevel(source)<=0)continue;
            VECTOR3 position{},horizon{},direction{};
            sdk->GetThrusterRef(source,position);sdk->HorizonRot(position,horizon);
            require(std::abs(horizon.y+sdk->GetAltitude(ALTMODE_GROUND)-0.1)<0.01,
                    "active lunar dust source remains just above terrain");
            sdk->GetThrusterDir(source,direction);sdk->HorizonRot(direction,horizon);
            // Check tangent alignment within 0.006 degrees in the runtime horizon frame.
            require(std::abs(horizon.y)<1e-4,"lunar dust source direction is tangent to the surface");
            ++dustSources;
        }
        require(dustSources==4,"all four LM dust sources follow the local ground plane");
        size_t particles=0,texturedParticles=0;
        require(particleStats(&particles,&texturedParticles) && particles>0,"real LM emits lunar dust during a low-altitude DPS burn");
        require(texturedParticles==particles,"LM dust resolves its custom texture");
        std::cout<<"LM dust particles="<<particles<<" textured="<<texturedParticles<<'\n';
        using ShowParticles=void (*)(bool);
        auto showParticles=reinterpret_cast<ShowParticles>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_particles_visible"));
        require(showParticles,"particle visibility diagnostic");
        VECTOR3 observer{},look{};
        sdk->HorizonInvRot(_V(0,20,30),observer);sdk->HorizonInvRot(_V(0,-20,-30),look);
        sdk->SetCameraOffset(observer);sdk->SetCameraDefaultDirection(unit(look));
        g_camera->ResetCockpitDir(false);g_camera->Update();
        auto dustImage=[&]() {
            host.RenderNativeFrame();std::vector<BYTE> image(size_t(host.ViewW())*host.ViewH()*4);
            require(read(image.data(),image.size(),host.ViewW()*4),"read real LM dust scene");return image;
        };
        const auto visibleDust=dustImage();showParticles(false);const auto noDust=dustImage();showParticles(true);
        size_t dustPixels=0;
        for(size_t i=0;i<visibleDust.size();i+=4)
            if(!std::equal(visibleDust.begin()+i,visibleDust.begin()+i+3,noDust.begin()+i))++dustPixels;
        std::cout<<"Visible LM dust pixels="<<dustPixels<<'\n';
        require(dustPixels>100,"real LM dust contributes visible pixels against lunar terrain");
        auto &particleSetting=host.Cfg()->CfgVisualPrm.bParticleStreams;
        const bool originalParticleSetting=particleSetting;
        particleSetting=false;
        require(dustImage()==noDust,"particle setting hides existing dust immediately");
        size_t hiddenParticles=0,hiddenTextured=0;
        require(particleStats(&hiddenParticles,&hiddenTextured) && hiddenParticles==particles &&
                hiddenTextured==texturedParticles,"hiding dust preserves simulated particles");
        particleSetting=true;
        require(dustImage()==visibleDust,"re-enabling particles restores the exact dust frame");
        particleSetting=originalParticleSetting;
        if(const char *path=std::getenv("ORBITER_DUST_CAPTURE")) {
            std::ofstream image(path,std::ios::binary);image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<visibleDust.size();i+=4)image.write(reinterpret_cast<const char *>(visibleDust.data()+i),3);
            require(bool(image),"save LM dust capture");
        }
        require(host.LoadNativeVirtualCockpit(0),"restore cockpit after dust observer view");
        sdk->DefSetStateEx(&orbit);step(180);
        require(particleStats(&particles,&texturedParticles) && particles==0,"LM dust expires after returning above the altitude cutoff");
        const double high=sdk->GetThrusterLevel(engine);
        key(SDL_SCANCODE_PERIOD,true);step(120);key(SDL_SCANCODE_PERIOD,false);step(60);
        require(sdk->GetThrusterLevel(engine)<high-0.1,"TTCA reduces actual descent thrust");
        // Losing window focus must release throttle velocity without stopping the burn.
        key(SDL_SCANCODE_M,true);step(10);
        SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;
        require(SDL_PushEvent(&lost) && window.Poll(),"release LM throttle on focus loss");
        step(60);const double released=sdk->GetThrusterLevel(engine);step(120);
        require(std::abs(sdk->GetThrusterLevel(engine)-released)<0.01,"released throttle does not keep climbing");
        key(SDL_SCANCODE_LEFTBRACKET,true);key(SDL_SCANCODE_LEFTBRACKET,false);step(120);
        require(sdk->GetThrusterLevel(engine)<0.001,"manual stop shuts down descent engine");
        g_camera->Attach(vessel,1);g_camera->Update();
        const auto stopped=exhaustImage();showExhaust(false);
        require(exhaustImage()==stopped,"stopped engines leave no stale exhaust geometry");
        showExhaust(true);
        require(host.LoadNativeVirtualCockpit(4),"left panel staging controls");host.RenderNativeFrame();
        press(3812,0.2); // MASTER ARM
        require(state().find("EDMasterArm 1 0")!=std::string::npos,"arm explosive devices");
        press(3818,0.2); // ASCENT HELIUM PRESSURIZATION
        press(3828,0.5,SDL_BUTTON_RIGHT); // open STAGE cover
        require(state().find("EDStage 0 1")!=std::string::npos,"open guarded staging switch");
        const double combinedMass=sdk->GetMass();
        press(3828,0.2);step(60);
        require(oapiGetVesselCount()==2,"staging creates descent-stage vessel");
        auto descent=oapiGetVesselByName("Eagle-DESCENTSTG");
        require(descent && !oapiGetVesselInterface(descent)->GetEnableFocus(),"descent stage is non-focusable");
        require(sdk->GetMass()<combinedMass-1000,"ascent vessel sheds descent-stage mass");
        require(host.LoadNativeVirtualCockpit(0),"cockpit survives staging");host.RenderNativeFrame();
        require(areas.owner==oapiGetFocusObject() && !areas.areas.empty(),"ascent cockpit controls remain registered");
        require(read(pixels.data(),pixels.size(),host.ViewW()*4),"read ascent cockpit after staging");
        visible=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]+pixels[i+1]+pixels[i+2]>30)++visible;
        require(visible>10000,"ascent cockpit geometry remains visible");
        require(host.SaveScenario("NativeLMStaged","Native LM separated stages",0),"save separated spacecraft");
        press(3112,0.2);press(3112,0.2); // ENG ARM: DES -> OFF -> ASC
        require(state().find("EngineArmSwitch 2 0")!=std::string::npos,"arm ascent engine through cockpit");
        THRUSTER_HANDLE ascent=nullptr;
        for(DWORD i=0;i<sdk->GetThrusterCount();++i) {
            auto t=sdk->GetThrusterHandleByIndex(i);
            if(sdk->GetThrusterMax(t)>10000) {require(!ascent,"one ascent engine");ascent=t;}
        }
        require(ascent && sdk->GetThrusterLevel(ascent)==0,"ascent engine initially off");
        const double ascentFuel=sdk->GetTotalPropellantMass();
        key(SDL_SCANCODE_RIGHTBRACKET,true);step(30);key(SDL_SCANCODE_RIGHTBRACKET,false);step(30);
        require(sdk->GetThrusterLevel(ascent)<0.001,"latched engine stop inhibits restart");
        VECTOR3 beforeVelocity{},beforeDescentVelocity{};
        sdk->GetGlobalVel(beforeVelocity);oapiGetVesselInterface(descent)->GetGlobalVel(beforeDescentVelocity);
        key(SDL_SCANCODE_LEFTBRACKET,true);key(SDL_SCANCODE_LEFTBRACKET,false);step(2);
        require(state().find("CDRManualEngineStop 0 0")!=std::string::npos,"release latched engine stop before restart");
        key(SDL_SCANCODE_RIGHTBRACKET,true);step(30);key(SDL_SCANCODE_RIGHTBRACKET,false);step(120);
        std::cout<<"APS ignition level "<<sdk->GetThrusterLevel(ascent)<<'\n';

        require(sdk->GetThrusterLevel(ascent)>0.9,"manual start ignites ascent engine");
        require(sdk->GetTotalPropellantMass()<ascentFuel-1.0,"ascent engine consumes propellant");
        VECTOR3 afterVelocity{},afterDescentVelocity{};
        sdk->GetGlobalVel(afterVelocity);oapiGetVesselInterface(descent)->GetGlobalVel(afterDescentVelocity);
        require(length((afterVelocity-afterDescentVelocity)-(beforeVelocity-beforeDescentVelocity))>1.0,"ascent thrust accelerates Eagle away from spent descent stage");
        key(SDL_SCANCODE_PERIOD,true);step(120);key(SDL_SCANCODE_PERIOD,false);
        require(sdk->GetThrusterLevel(ascent)>0.99,"ascent engine remains fixed-thrust despite TTCA command");
        require(host.SaveScenario("NativeLMAscent","Native LM ascent engine burning",0),"save active ascent burn");
        key(SDL_SCANCODE_LEFTBRACKET,true);key(SDL_SCANCODE_LEFTBRACKET,false);step(120);
        require(sdk->GetThrusterLevel(ascent)<0.001,"manual stop shuts down ascent engine");
        std::cout<<"Native Eagle cockpit, twelve views, dynamic DSKY, LGC and manual descent/ascent propulsion verified\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
