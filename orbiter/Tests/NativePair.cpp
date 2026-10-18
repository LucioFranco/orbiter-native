#include "Orbiter.h"
#include "Config.h"
#include "Vessel.h"
#include "Camera.h"
#include "NativeCockpit.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <filesystem>
#include <sstream>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstdlib>
extern Camera *g_camera;
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
static std::string display(Vessel *vessel) {
    std::ostringstream out;vessel->Write(out);auto s=out.str();auto a=s.find("DSKY_BEGIN"),b=s.find("DSKY_END",a);
    require(a!=std::string::npos && b!=std::string::npos,"vessel saves its DSKY");return s.substr(a,b-a);
}
static int pitchChannel(Vessel *vessel) {
    std::ostringstream out;vessel->Write(out);auto s=out.str();auto a=s.find("VICHAN025 ");
    require(a!=std::string::npos,"AGC input channel saved");return std::stoi(s.substr(a+10)) & 3;
}
int main(int argc,char **argv) {
    try {
        require(argc==3 || argc==4,"graphics plugin and pair runtime required");
        const auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);
        Orbiter host;auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());
        require(module && host.GetGraphicsClient()->clbkInitialise(),"initialize graphics");
        const bool padDay=argc==4 && std::string(argv[3])=="--pad-day";
        const bool pad=padDay || (argc==4 && std::string(argv[3])=="--pad");
        host.Launch(pad?"NasspPad":"NasspPair");require(oapiGetVesselCount()==(pad?3:2),"scene vessels loaded");
        auto csm=reinterpret_cast<Vessel *>(oapiGetVesselByName("Columbia"));
        char secondName[6];std::snprintf(secondName,sizeof(secondName),"%s",pad?"ML":"Eagle");
        auto lm=reinterpret_cast<Vessel *>(oapiGetVesselByName(secondName));require(csm && lm && csm!=lm,"distinct vessel instances");
        require(host.LoadNativeVirtualCockpit(),"load Columbia VC");
        orbiter::platform::SDLWindow window(host);
        auto &areas=orbiter::NativeCockpitState();
        auto step=[&](int n) {for(int i=0;i<n;++i)host.StepSimulation(1.0/60.0);};
        using Read=bool (*)(BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));require(read,"framebuffer reader");
        auto pixels=[&]() {host.RenderNativeFrame();std::vector<BYTE> out(size_t(host.ViewW())*host.ViewH()*4);require(read(out.data(),out.size(),host.ViewW()*4),"read active cockpit");return out;};
        if(pad || (argc==4 && std::string(argv[3])=="--exterior")) {
            if(pad)step(1);
            for(auto vessel : (pad?std::vector<Vessel *>{csm}:std::vector<Vessel *>{csm,lm})) {
                g_camera->Attach(vessel,1);g_camera->SetDistance(vessel==lm?1.5:3);g_camera->AddPhi(PI*0.5);g_camera->Update();
                if(pad) {
                    char earthName[]="Earth";
                    g_camera->SetGroundMode(CAMERA_GROUNDOBSERVER,reinterpret_cast<Body *>(oapiGetGbodyByName(earthName)),
                        -80.6075*RAD,28.6083*RAD,25);
                    g_camera->SetGroundObserver_TargetLock(true);g_camera->Update();
                }
                const auto visible=pixels();
                if(!pad) {
                    auto &specular=host.Cfg()->CfgVisualPrm.bSpecular;const bool saved=specular;
                    specular=true;const auto glossy=pixels();
                    specular=false;const auto matte=pixels();
                    specular=saved;
                    size_t highlights=0;
                    for(size_t i=0;i<glossy.size();i+=4)
                        if(int(glossy[i])+glossy[i+1]+glossy[i+2]>int(matte[i])+matte[i+1]+matte[i+2]+3)++highlights;
                    std::cout<<"Exterior material highlight pixels="<<highlights<<'\n';
                    require(highlights>0,"original reflective spacecraft materials contribute visible highlights");
                    // The LM has only a small real highlight in this view. Its old
                    // large minimum also counted incorrect zero-power reflections.
                    // Instead verify that disabling reflection in the actual device
                    // materials matches the renderer toggle, then restore exactly.
                    struct SavedReflection {DEVMESHHANDLE mesh;DWORD index;MATERIAL value;};
                    std::vector<SavedReflection> reflections;
                    auto graphics=host.GetGraphicsClient();
                    auto sdk=oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(vessel));
                    for(UINT i=0;i<sdk->GetMeshCount();++i) {
                        auto mesh=reinterpret_cast<DEVMESHHANDLE>(graphics->clbkGetMesh(vessel->GetVishandle(),i));
                        if(!mesh)continue;
                        for(DWORD j=0;;++j) {
                            MATERIAL value{};if(graphics->clbkMeshMaterial(mesh,j,&value))break;
                            reflections.push_back({mesh,j,value});auto noReflection=value;noReflection.power=0;
                            require(!graphics->clbkSetMeshMaterial(mesh,j,&noReflection),"disable reflection in the device material");
                        }
                    }
                    require(!reflections.empty(),"exterior reflection check uses real device materials");
                    specular=true;require(pixels()==matte,"zero-power device materials match the specular-disabled exterior");
                    for(const auto &entry:reflections)
                        require(!graphics->clbkSetMeshMaterial(entry.mesh,entry.index,&entry.value),"restore original exterior material");
                    require(pixels()==glossy,"restoring device materials restores the exact original highlights");
                    specular=saved;
                    require(pixels()==visible,"restoring specular setting restores exact exterior frame");
                }
                if(pad && !padDay) {
                    char floodName[]="Floodlight";auto flood=oapiGetVesselInterface(oapiGetVesselByName(floodName));
                    require(flood && flood->LightEmitterCount()==6,"original pad floodlights initialized");
                    for(DWORD i=0;i<flood->LightEmitterCount();++i)const_cast<LightEmitter *>(flood->GetLightEmitter(i))->Activate(false);
                    const auto dark=pixels();size_t litPixels=0;
                    for(size_t i=0;i<visible.size();i+=4)if(int(visible[i])+visible[i+1]+visible[i+2]>int(dark[i])+dark[i+1]+dark[i+2]+15)++litPixels;
                    require(litPixels>1000,"separate floodlight vessel illuminates launch scene");
                    for(DWORD i=0;i<flood->LightEmitterCount();++i)const_cast<LightEmitter *>(flood->GetLightEmitter(i))->Activate(true);
                    std::cout<<"Pad floodlights: "<<litPixels<<" illuminated pixels\n";
                    std::vector<std::pair<VESSEL *,std::vector<WORD>>> hiddenVessels;
                    for(auto object : {csm,lm}) {
                        auto receiver=oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(object));std::vector<WORD> modes;
                        for(UINT i=0;i<receiver->GetMeshCount();++i) {modes.push_back(receiver->GetMeshVisibilityMode(i));receiver->SetMeshVisibilityMode(i,MESHVIS_NEVER);}
                        hiddenVessels.push_back({receiver,modes});
                    }
                    // Original pad lights aim upward. Temporarily point one at
                    // the surface to verify terrain reception without altering
                    // NASSP's scene configuration or its floodlight logic.
                    auto groundLight=const_cast<LightEmitter *>(flood->GetLightEmitter(0));
                    const auto originalDirection=groundLight->GetDirection();
                    VECTOR3 downward{};flood->HorizonInvRot(_V(0,-1,0),downward);
                    groundLight->SetDirection(downward);
                    const auto groundLit=pixels();
                    for(DWORD i=0;i<flood->LightEmitterCount();++i)const_cast<LightEmitter *>(flood->GetLightEmitter(i))->Activate(false);
                    const auto groundDark=pixels();size_t groundPixels=0;
                    for(size_t i=0;i<groundLit.size();i+=4)if(int(groundLit[i])+groundLit[i+1]+groundLit[i+2]>int(groundDark[i])+groundDark[i+1]+groundDark[i+2]+3)++groundPixels;
                    std::cout<<"Ground floodlights: "<<groundPixels<<" illuminated terrain pixels"<<std::endl;
                    require(groundPixels>100,"pad floodlights illuminate terrain independently of vessel meshes");
                    groundLight->SetDirection(originalDirection);
                    for(DWORD i=0;i<flood->LightEmitterCount();++i)const_cast<LightEmitter *>(flood->GetLightEmitter(i))->Activate(true);
                    for(const auto &[receiver,modes]:hiddenVessels)for(UINT i=0;i<modes.size();++i)receiver->SetMeshVisibilityMode(i,modes[i]);
                }
                if(padDay && std::getenv("ORBITER_CHECK_PAD_SHADOWS")) {
                    auto &visual=host.Cfg()->CfgVisualPrm;
                    const bool bases=visual.bShadows,vessels=visual.bVesselShadows;
                    visual.bShadows=false;visual.bVesselShadows=false;
                    const auto unshadowed=pixels();size_t changed=0;
                    for(size_t i=0;i<visible.size();i+=4)
                        if(int(unshadowed[i])+unshadowed[i+1]+unshadowed[i+2]>int(visible[i])+visible[i+1]+visible[i+2]+6)++changed;
                    require(changed>100,"pad sunlight shadow toggle changes visible pixels");
                    std::ofstream capture("pad-no-shadows.ppm",std::ios::binary);
                    capture<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
                    for(size_t i=0;i<unshadowed.size();i+=4)capture.write(reinterpret_cast<const char *>(unshadowed.data()+i),3);
                    require(bool(capture),"save unshadowed pad comparison");
                    std::cout<<"Pad sunlight shadows: "<<changed<<" darkened pixels"<<std::endl;
                    if(std::getenv("ORBITER_CHECK_TERRAIN_SEAMS")) {
                        SDL_setenv_unsafe("ORBITER_DIAGNOSTIC_UNLIT_TERRAIN","1",1);
                        const auto unlit=pixels();
                        SDL_unsetenv_unsafe("ORBITER_DIAGNOSTIC_UNLIT_TERRAIN");
                        std::ofstream terrain("pad-unlit-terrain.ppm",std::ios::binary);
                        terrain<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
                        for(size_t i=0;i<unlit.size();i+=4)terrain.write(reinterpret_cast<const char *>(unlit.data()+i),3);
                        require(bool(terrain),"save unlit terrain comparison");
                    }
                    visual.bShadows=bases;visual.bVesselShadows=vessels;
                    require(pixels()==visible,"restoring shadow settings restores the exact pad frame");
                }
                auto sdk=oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(vessel));
                std::vector<WORD> modes;
                for(UINT i=0;i<sdk->GetMeshCount();++i) {modes.push_back(sdk->GetMeshVisibilityMode(i));sdk->SetMeshVisibilityMode(i,MESHVIS_NEVER);}
                const auto hidden=pixels();size_t changed=0;
                for(size_t i=0;i<visible.size();i+=4)if(visible[i]!=hidden[i] || visible[i+1]!=hidden[i+1] || visible[i+2]!=hidden[i+2])++changed;
                require(changed>1000,"target spacecraft exterior contributes visible pixels");
                for(UINT i=0;i<modes.size();++i)sdk->SetMeshVisibilityMode(i,modes[i]);
                std::ofstream image(std::string(vessel->Name())+"-exterior.ppm",std::ios::binary);
                image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
                for(size_t i=0;i<visible.size();i+=4)image.write(reinterpret_cast<const char *>(visible.data()+i),3);
                require(bool(image),"save exterior capture");
                std::cout<<vessel->Name()<<" exterior: "<<changed<<" visible spacecraft pixels\n";
            }
            require(host.LoadNativeVirtualCockpit(),"restore cockpit after exterior views");pixels();
            return 0;
        }
        if(argc==4) {
            char moonName[]="Moon";VECTOR3 moon{},eye{};
            oapiGetGlobalPos(oapiGetGbodyByName(moonName),&moon);oapiCameraGlobalPos(&eye);
            Vector direction=tmul(csm->GRot(),Vector(moon.x-eye.x,moon.y-eye.y,moon.z-eye.z));direction=direction/direction.length();
            g_camera->SetDefaultCockpitDir(direction);g_camera->SetCockpitDir(0,0);g_camera->Update();
            auto csmInterface=oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(csm));
            std::vector<WORD> visibility;
            for(UINT i=0;i<csmInterface->GetMeshCount();++i){visibility.push_back(csmInterface->GetMeshVisibilityMode(i));csmInterface->SetMeshVisibilityMode(i,MESHVIS_NEVER);}
            const auto frame=pixels();
            std::ofstream capture("terrain-frame.rgba",std::ios::binary);
            capture.write(reinterpret_cast<const char *>(frame.data()),frame.size());
            require(bool(capture),"save terrain comparison frame");
            std::ofstream ppm("terrain-frame.ppm",std::ios::binary);
            ppm<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<frame.size();i+=4)ppm.write(reinterpret_cast<const char *>(frame.data()+i),3);
            require(bool(ppm),"save terrain inspection frame");
            using Stats=bool (*)(size_t *,size_t *,size_t *);
            auto stats=reinterpret_cast<Stats>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_terrain_stats"));
            size_t meshes=0,textured=0,textures=0;
            require(stats && stats(&meshes,&textured,&textures),"read terrain residency diagnostics");
            if(std::string(argv[3])=="--terrain-assets")
                require(textures>0 && textured==meshes,"downloaded Earth/Moon textures cover every selected terrain tile");
            else
                require(textures>=2 && textures<=4 && textured==meshes && textured>textures,"visible synthetic Earth/Moon tiles share their hemisphere root textures");
            std::cout<<"Terrain residency: "<<meshes<<" meshes, "<<textured<<" textured, "<<textures<<" resident DDS textures"<<std::endl;
            for(UINT i=0;i<visibility.size();++i)csmInterface->SetMeshVisibilityMode(i,visibility[i]);
            require(host.LoadNativeVirtualCockpit(),"restore cockpit after terrain capture");
            // The ordinary native_pair test covers the full flight-computer,
            // focus and material regression. Terrain fixtures run this capture
            // repeatedly and should not repeat that independent suite each time.
            return 0;
        }
        auto key=[&](SDL_Scancode code,bool down) {SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.scancode=code;e.key.down=down;require(SDL_PushEvent(&e) && window.Poll(),"dispatch vessel key");};
        auto cycle=[&]() {key(SDL_SCANCODE_F4,true);key(SDL_SCANCODE_F4,false);};
        auto press=[&](int id) {
            const auto &area=areas.areas.at(id);auto point=area.points[0];
            if(area.shape==2){point=_V(0,0,0);for(const auto &p:area.points)point+=p*0.25;}
            auto active=reinterpret_cast<Vessel *>(oapiGetFocusObject());const auto eye=g_camera->CockpitPos();
            int x,y,ww,wh;require(g_camera->Direction2Viewport(mul(active->GRot(),Vector(point.x-eye.x,point.y-eye.y,point.z-eye.z)),x,y),"active DSKY key visible");
            SDL_GetWindowSize(window.Handle(),&ww,&wh);SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.down=true;
            e.button.x=float(x)*ww/host.ViewW();e.button.y=float(y)*wh/host.ViewH();require(SDL_PushEvent(&e) && window.Poll(),"active DSKY press");
            require(areas.capture[0] && areas.capture[0]->id==id,"active cockpit receives pick");step(30);
            e.type=SDL_EVENT_MOUSE_BUTTON_UP;e.button.down=false;require(SDL_PushEvent(&e) && window.Poll(),"DSKY release");step(30);
        };
        step(30);auto csmPixels=pixels();
        press(4801);press(4808);press(4810);
        require(display(csm).find("VERB 35")!=std::string::npos,"CMC receives its own keyboard input");
        require(display(lm).find("VERB 35")==std::string::npos,"CMC input does not reach LGC");
        require(host.LoadNativeVirtualCockpit(2),"remember CSM center seat");pixels();
        oapiGetFocusInterface()->SetAttitudeMode(RCS_ROT);
        key(SDL_SCANCODE_KP_2,true);step(2);
        require(pitchChannel(csm)!=3,"held CSM pitch reaches AGC");
        cycle();
        require(pitchChannel(csm)==3,"focus loss releases CSM pitch channel immediately");
        require(oapiGetFocusObject()==reinterpret_cast<OBJHANDLE>(lm) && areas.owner==reinterpret_cast<OBJHANDLE>(lm),"F4 hands control registry to Eagle");
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"focus change releases flight keys");
        auto lmPixels=pixels();require(lmPixels!=csmPixels && csm->GetVishandle() && lm->GetVishandle(),"renderer retains both vessel visuals across focus handoff");
        require(std::string(SDL_GetWindowTitle(window.Handle())).find("Eagle")!=std::string::npos,"window title identifies Eagle");
        require(host.LoadNativeVirtualCockpit(3),"remember LM DSKY seat");pixels();
        press(3401);press(3408);press(3410);press(3418);
        require(display(lm).find("PROG 88")!=std::string::npos,"LGC independently executes lamp test");
        require(display(csm).find("VERB 35")!=std::string::npos && display(csm).find("PROG 88")==std::string::npos,"LGC input does not execute CMC display test");
        oapiGetFocusInterface()->SetAttitudeMode(RCS_ROT);
        key(SDL_SCANCODE_KP_2,true);step(2);
        require(pitchChannel(lm)!=3,"held LM pitch reaches AGC");
        cycle();
        require(pitchChannel(lm)==3,"focus loss releases LM pitch channel immediately");
        cycle();
        oapiSetPause(true);step(1);const auto csmState=display(csm),lmState=display(lm);
        for(int i=0;i<4;++i) {
            cycle();pixels();
            const bool isCSM=oapiGetFocusObject()==reinterpret_cast<OBJHANDLE>(csm);
            require(areas.cockpitId==(isCSM?2:3),"each vessel retains its selected seat across focus changes");
            require(display(csm)==csmState && display(lm)==lmState,"paused focus switching preserves both computers");
        }
        // Bring the real CSM in front of Eagle's window without advancing either simulation.
        require(host.LoadNativeVirtualCockpit(0),"Eagle window view");pixels();
        auto other=oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(csm));
        VESSELSTATUS2 original{};original.version=2;other->GetStatusEx(&original);auto nearby=original;
        VECTOR3 cameraPosition{},oldPosition{};MATRIX3 cameraRotation{};
        oapiCameraGlobalPos(&cameraPosition);oapiCameraRotationMatrix(&cameraRotation);other->GetGlobalPos(oldPosition);
        nearby.rpos+=cameraPosition+mul(cameraRotation,_V(-40,-8,80))-oldPosition;
        other->DefSetStateEx(&nearby);
        const auto withCSM=pixels();
        std::vector<WORD> modes;
        for(UINT i=0;i<other->GetMeshCount();++i) {modes.push_back(other->GetMeshVisibilityMode(i));other->SetMeshVisibilityMode(i,MESHVIS_NEVER);}
        const auto withoutCSM=pixels();size_t changed=0;
        for(size_t i=0;i<withCSM.size();i+=4)if(!std::equal(withCSM.begin()+i,withCSM.begin()+i+3,withoutCSM.begin()+i))++changed;
        if(const char *path=std::getenv("ORBITER_PAIR_CAPTURE")) {
            std::ofstream file(path,std::ios::binary);file<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<withCSM.size();i+=4)file.write(reinterpret_cast<const char *>(withCSM.data()+i),3);
            require(bool(file),"save external vessel capture");
        }
        std::cout<<"External CSM pixels: "<<changed<<std::endl;
        require(changed>500,"real external CSM renders through Eagle's window");
        for(UINT i=0;i<modes.size();++i)other->SetMeshVisibilityMode(i,modes[i]);
        require(pixels()==withCSM,"external visibility updates restore identical spacecraft image");
        // Device material edits must reach the non-focused vessel's cache.
        struct SavedMaterial {DEVMESHHANDLE mesh;DWORD index;MATERIAL value;};
        std::vector<SavedMaterial> materials;auto graphics=host.GetGraphicsClient();
        for(UINT i=0;i<other->GetMeshCount();++i) {
            auto mesh=reinterpret_cast<DEVMESHHANDLE>(graphics->clbkGetMesh(csm->GetVishandle(),i));
            if(!mesh)continue;
            for(DWORD j=0;;++j) {
                MATERIAL value{};if(graphics->clbkMeshMaterial(mesh,j,&value))break;
                materials.push_back({mesh,j,value});auto changed=value;changed.diffuse={0.1f,1.0f,0.1f,value.diffuse.a};
                require(!graphics->clbkSetMeshMaterial(mesh,j,&changed),"edit non-focused CSM material");
            }
        }
        require(pixels()!=withCSM,"non-focused material edits update external spacecraft");
        for(const auto &material:materials)require(!graphics->clbkSetMeshMaterial(material.mesh,material.index,&material.value),"restore external materials");
        require(pixels()==withCSM,"external material restoration leaves focused cockpit unchanged");
        other->DefSetStateEx(&original);
        require(host.LoadNativeVirtualCockpit(3),"restore saved LM seat");pixels();
        // Disabled focus targets are skipped; a lone eligible vessel stays selected.
        oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(csm))->SetEnableFocus(false);
        require(!host.CycleNativeFocus() && oapiGetFocusObject()==reinterpret_cast<OBJHANDLE>(lm),"cycle skips disabled focus target");
        oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(csm))->SetEnableFocus(true);
        require(host.SaveScenario("NativePairSaved","Native CSM and LM",0),"save both spacecraft");
        std::cout<<"Combined CSM/LM simulation, independent computers, F4 focus handoff and per-vessel seats verified\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
