#include "Orbiter.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include "NativeCockpit.h"
#include "Camera.h"
#include "Vessel.h"
#include "Mesh.h"
extern Camera *g_camera;
static void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
int main(int argc,char **argv) {
    try {
        require(argc==3 || argc==4 || argc==5,"module path and staged runtime required; optional frames");
        std::filesystem::path modulePath=std::filesystem::absolute(argv[1]);
        std::filesystem::current_path(argv[2]);
        Orbiter host;
        auto module=host.LoadModule(modulePath.parent_path().string().c_str(),modulePath.stem().string().c_str());
        require(module && host.GetGraphicsClient() && host.GetGraphicsClient()->clbkInitialise(),"load GPU client");
        const bool csm=argc==5 && std::string(argv[4])=="--csm";
        const bool insertion=argc==5 && std::string(argv[4])=="--insertion";
        const bool powered=csm || insertion;
        const bool live=powered || (argc==5 && std::string(argv[4])=="--live");
        if (live) {host.Launch(csm?"NasspCSM":insertion?"NasspInsertion":"NasspStartup");require(host.LoadNativeVirtualCockpit(),"load running vessel cockpit");}
        else {
        auto mesh=oapiLoadMeshGlobal("ProjectApollo/CM-VC");
        require(mesh && oapiMeshGroupCount(mesh)==1157,"load real command-module cockpit");
        for (DWORD i=1;i<=37;++i) require(oapiGetTextureHandle(mesh,i)!=nullptr,"load every cockpit texture");
        using Preview=bool (*)(MESHHANDLE,const float *,DWORD,DWORD);
        auto preview=reinterpret_cast<Preview>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_preview_mesh"));
        require(preview!=nullptr,"mesh preview export");
        // NASSP commander seat, local to the cockpit mesh. +Z forward, +Y up.
        // Subtract camera position in double precision before float projection.
        const double tilt=18.4*3.141592653589793/180.0,c=std::cos(tilt),s=std::sin(tilt);
        // saturnvc.cpp subtracts 0.15 m from the mesh offset before adding 0.1 m.
        const double camera[3]={-0.6,0.85,-0.05};
        double view[16]={1,0,0,0, 0,c,-s,0, 0,s,c,0,
            -camera[0],-c*camera[1]-s*camera[2],s*camera[1]-c*camera[2],1};
        const double f=1/std::tan(60.0*3.141592653589793/360.0);
        const double projection[16]={f/(1280.0/720.0),0,0,0, 0,f,0,0, 0,0,0,1, 0,0,0.01,0};
        float mvp[16]{};
        for(int column=0;column<4;++column)for(int row=0;row<4;++row) {
            double value=0;for(int k=0;k<4;++k)value+=projection[k*4+row]*view[column*4+k];
            mvp[column*4+row]=float(value);
        }
        require(preview(mesh,mvp,1280,720),"upload cockpit geometry");
        }
        {
            orbiter::platform::SDLWindow window(host);
            using ReadOutput=bool (*)(BYTE *,size_t,DWORD);
            auto read=reinterpret_cast<ReadOutput>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));
            std::vector<BYTE> before,after;
            VECTOR3 beforeDirection{};
            if (live) {
                require(oapiCockpitMode()==COCKPIT_VIRTUAL,"native VC mode");
                require(read!=nullptr,"output readback hook");
                host.StepSimulation(1.0/60.0);host.RenderNativeFrame();
                if(csm) {
                    using ShadowGroups=size_t (*)();
                    auto shadowGroups=reinterpret_cast<ShadowGroups>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_shadow_only_groups"));
                    require(shadowGroups && shadowGroups()>0,"native CSM exterior hull casts shadows inside cockpit");
                    std::cout<<"Cockpit exterior shadow groups: "<<shadowGroups()<<std::endl;
                }
                using StarCount=size_t (*)();
                auto starCount=reinterpret_cast<StarCount>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_star_count"));
                require(starCount && starCount()==41057,"live scene loads Orbiter star catalog");
                auto focus=reinterpret_cast<Vessel *>(oapiGetFocusObject());
                auto visual=focus->GetVishandle();require(visual!=nullptr,"native vessel visual registered");
                auto graphics=host.GetGraphicsClient();size_t hiddenChanges=0;
                auto vesselInterface=oapiGetVesselInterface(oapiGetFocusObject());
                using OutputMesh=uint64_t (*)();
                auto outputMesh=reinterpret_cast<OutputMesh>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_output_mesh"));
                require(outputMesh && outputMesh(),"cockpit mesh identity");
                // NASSP repeats mesh visibility notifications and device edits.
                // Ordinary steps must reuse the cockpit topology and bindings.
                host.StepSimulation(1.0/60.0);host.RenderNativeFrame();
                const auto stableMesh=outputMesh();
                for(int frame=0;frame<4;++frame) {
                    host.StepSimulation(1.0/60.0);host.RenderNativeFrame();
                    require(outputMesh()==stableMesh,"steady cockpit updates preserve mesh allocation");
                }

                for(UINT meshIndex=0;meshIndex<vesselInterface->GetMeshCount();++meshIndex) {
                    auto source=static_cast<Mesh *>(vesselInterface->GetMeshTemplate(meshIndex));
                    auto owned=static_cast<Mesh *>(graphics->clbkGetMesh(visual,meshIndex));
                    if(!source)continue;
                    require(owned && owned!=source,"visual owns independent mesh copy");
                    for(DWORD group=0;group<source->nGroup();++group)
                        if(source->GetGroupUsrFlag(group)!=owned->GetGroupUsrFlag(group))++hiddenChanges;
                    if(source->nMaterial()) {
                        MATERIAL original{},changed{};auto handle=reinterpret_cast<DEVMESHHANDLE>(owned);
                        require(graphics->clbkMeshMaterial(handle,0,&original)==0,"read visual material");
                        changed=original;changed.diffuse.r=0.123f;
                        require(graphics->clbkSetMeshMaterial(handle,0,&changed)==0,"write visual material");
                        require(source->GetMaterial(0)->diffuse.r==original.diffuse.r && owned->GetMaterial(0)->diffuse.r==changed.diffuse.r,"material change isolated from template");
                        require(graphics->clbkSetMeshMaterial(handle,0,&original)==0,"restore visual material");
                        oapi::FVECTOR4 oldSpecular{},specular{.2f,.3f,.4f,48.f},specularResult{};
                        require(graphics->clbkMeshMaterialEx(handle,0,Specular,&oldSpecular)==0,"read original specular material");
                        require(graphics->clbkSetMeshMaterialEx(handle,0,Specular,&specular)==0,"set specular colour and power");
                        require(graphics->clbkMeshMaterialEx(handle,0,Specular,&specularResult)==0 &&
                                specularResult.g==specular.g && specularResult.a==specular.a,"specular material round trip");
                        require(source->GetMaterial(0)->power==original.power,"specular changes leave NASSP mesh template intact");
                        require(graphics->clbkSetMeshMaterialEx(handle,0,Specular,&oldSpecular)==0,"restore original specularity");
                        oapi::FVECTOR4 oldLight{},roundtrip{};
                        require(graphics->clbkMeshMaterialEx(handle,0,Light,&oldLight)==0,"read added-light property");
                        const oapi::FVECTOR4 light{0.2f,0.3f,0.4f,1.0f};
                        require(graphics->clbkSetMeshMaterialEx(handle,0,Light,&light)==0,"set added-light property");
                        require(graphics->clbkMeshMaterialEx(handle,0,Light,&roundtrip)==0 && roundtrip.g==light.g,"extended material round trip");
                        require(source->GetMaterial(0)->emissive.g==original.emissive.g,"added light isolated from mesh template");
                        require(graphics->clbkSetMeshMaterialEx(handle,0,Light,&oldLight)==0,"restore added light");
                        require(graphics->clbkSetMeshMaterialEx(handle,0,Metal,&light)==2,"unsupported material property reported");
                    }
                    if(source->nGroup() && source->GetGroup(0)->nVtx) {
                        const auto original=source->GetGroup(0)->Vtx[0];
                        NTVERTEX vertex{};GROUPREQUESTSPEC request{};request.Vtx=&vertex;request.nVtx=1;
                        auto handle=reinterpret_cast<DEVMESHHANDLE>(owned);
                        require(graphics->clbkGetMeshGroup(handle,0,&request)==0,"device mesh vertex read");
                        const auto saved=vertex;vertex.x+=0.25f;
                        GROUPEDITSPEC edit{};edit.flags=GRPEDIT_VTXCRDX;edit.Vtx=&vertex;edit.nVtx=1;
                        require(graphics->clbkEditMeshGroup(handle,0,&edit)==0,"device vertex edit");
                        require(owned->GetGroup(0)->Vtx[0].x==vertex.x && source->GetGroup(0)->Vtx[0].x==original.x,"vertex edit isolated from template");
                        vertex=saved;require(graphics->clbkEditMeshGroup(handle,0,&edit)==0,"restore device vertex");
                    }
                }
                require(hiddenChanges>0,"NASSP visual-created callback hides mission-specific groups");
                std::cout<<hiddenChanges<<" mesh groups changed only in the vessel visual\n";
                auto &areas=orbiter::NativeCockpitState();
                require(areas.areas.size()>100,"NASSP registers real VC areas");
                using ReadSurface=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
                auto readSurface=reinterpret_cast<ReadSurface>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));
                require(readSurface!=nullptr,"instrument readback");
                const auto &dsky=areas.areas.at(4942);
                require(dsky.redraws>0 && dsky.surface && dsky.target && dsky.background,"DSKY redraw initialized");
                const DWORD dw=dsky.rect.right-dsky.rect.left,dh=dsky.rect.bottom-dsky.rect.top;
                std::vector<BYTE> instrument(size_t(dw)*dh*4),background(instrument.size()),mapped(instrument.size());
                require(readSurface(dsky.surface,instrument.data(),instrument.size(),dw*4),"DSKY rendered pixels");
                require(readSurface(dsky.background,background.data(),background.size(),dw*4),"DSKY original background");
                if(powered)require(instrument!=background,"saved orbital state paints powered DSKY segments");
                for(size_t pixel=0;pixel<instrument.size();pixel+=4)
                    require(!(instrument[pixel]>240 && instrument[pixel+1]<16 && instrument[pixel+2]>240),"DSKY colour-key masks remain transparent");
                // An unpowered DSKY can correctly repaint only its background.
                auto gc=host.GetGraphicsClient();
                size_t changedAreas=0;
                for(const auto &[id,area]:areas.areas) {
                    if(!area.surface || !area.background || !area.redraws)continue;
                    const DWORD w=area.rect.right-area.rect.left,h=area.rect.bottom-area.rect.top;
                    std::vector<BYTE> current(size_t(w)*h*4),original(current.size());
                    require(readSurface(area.surface,current.data(),current.size(),w*4),"instrument scratch readback");
                    require(readSurface(area.background,original.data(),original.size(),w*4),"instrument background readback");
                    if(current!=original) {
                        ++changedAreas;
                        auto tile=gc->clbkCreateSurfaceEx(w,h,0);
                        require(tile && gc->clbkBlt(tile,0,0,area.target,area.rect.left,area.rect.top,w,h),"crop repainted atlas region");
                        std::vector<BYTE> atlas(current.size());
                        require(readSurface(tile,atlas.data(),atlas.size(),w*4),"read repainted atlas region");
                        require(atlas==current,"NASSP instrument pixels reach mesh texture");
                        gc->clbkReleaseSurface(tile);
                    }
                }
                require(changedAreas>0,"NASSP redraw callbacks paint cockpit instruments");
                std::cout<<changedAreas<<" cockpit areas painted by NASSP redraw callbacks\n";
                auto crop=gc->clbkCreateSurfaceEx(dw,dh,0);
                require(crop && gc->clbkBlt(crop,0,0,dsky.target,dsky.rect.left,dsky.rect.top,dw,dh),"crop cockpit atlas");
                require(readSurface(crop,mapped.data(),mapped.size(),dw*4),"read cockpit atlas crop");
                require(mapped==instrument,"redraw reaches cockpit texture region");gc->clbkReleaseSurface(crop);
                require(gc->clbkFillSurface(dsky.surface,0xff0000),"overwrite scratch to test background restoration");
                host.RenderNativeFrame();
                require(readSurface(dsky.surface,mapped.data(),mapped.size(),dw*4),"read restored scratch");
                require(mapped==instrument && dsky.redraws>=2,"each redraw restores background before drawing");
                auto &requested=areas.areas.at(4942);
                const auto savedDraw=requested.draw;requested.draw=PANEL_REDRAW_USER;
                const auto count=requested.redraws;
                oapiVCTriggerRedrawArea(99,4942);host.RenderNativeFrame();
                require(requested.redraws==count,"redraw request for inactive cockpit ignored");
                vesselInterface->TriggerRedrawArea(-1,-1,4942);host.RenderNativeFrame();
                require(requested.redraws==count+1,"vessel requests one cockpit redraw");
                host.RenderNativeFrame();require(requested.redraws==count+1,"user redraw consumed once");
                requested.draw=savedDraw;
                requested.mapping=PANEL_MAP_BGONREQUEST;
                require(gc->clbkFillSurface(requested.surface,0xff0000),"overwrite requested background destination");
                require(oapiBltPanelAreaBackground(4942,requested.surface),"SDK copies background on request");
                require(readSurface(requested.surface,mapped.data(),mapped.size(),dw*4) && mapped==background,"requested copy matches original background");
                requested.mapping=PANEL_MAP_BACKGROUND;host.RenderNativeFrame();
                auto vessel=reinterpret_cast<Vessel *>(oapiGetFocusObject());
                auto equipment=[&]() {
                    std::ostringstream saved;vessel->Write(saved);
                    const auto at=saved.str().find("CREWEQUIPMENTSTATE ");
                    require(at!=std::string::npos,"save crew equipment state");
                    return std::stoul(saved.str().substr(at+19));
                };
                const DWORD width=host.ViewW(),height=host.ViewH();
                before.resize(size_t(width)*height*4);after.resize(before.size());
                require(read(before.data(),before.size(),width*4),"initial animated cockpit");
                if(const auto path=std::getenv("ORBITER_COCKPIT_CAPTURE")) {
                    std::ofstream image(path,std::ios::binary);
                    image<<"P6\n"<<width<<' '<<height<<"\n255\n";
                    for(size_t pixel=0;pixel<before.size();pixel+=4)image.write(reinterpret_cast<const char *>(before.data()+pixel),3);
                    require(bool(image),"write requested cockpit capture");
                }
                const auto materialBaseline=before;const auto materialMesh=outputMesh();
                // Isolate the SDK material update from NASSP's per-frame lighting callbacks.
                std::vector<std::pair<int,int>> drawMasks;
                for(auto &[id,area]:areas.areas) {drawMasks.emplace_back(id,area.draw);area.draw=0;}
                // Exercise real NASSP emitters without a simulation step overwriting intensity.
                std::vector<std::pair<LightEmitter *,double>> emitters;
                for(DWORD i=0;i<vesselInterface->LightEmitterCount();++i) {
                    auto emitter=const_cast<LightEmitter *>(vesselInterface->GetLightEmitter(i));
                    if(emitter->GetType()==LightEmitter::LT_POINT && emitter->IsActive() &&
                       (emitter->GetVisibility() & LightEmitter::VIS_COCKPIT)) {
                        emitters.emplace_back(emitter,emitter->GetIntensity());emitter->SetIntensity(0);
                    }
                }
                require(emitters.size()==3,"NASSP registers three cockpit floodlights");
                host.RenderNativeFrame();
                require(read(before.data(),before.size(),width*4),"cockpit with floodlights off");
                const auto unlit=before;
                for(auto &[emitter,intensity]:emitters)emitter->SetIntensity(2);
                host.RenderNativeFrame();
                require(read(after.data(),after.size(),width*4),"cockpit with floodlights on");
                size_t illuminated=0;
                for(size_t p=0;p<before.size();p+=4)
                    if(int(after[p])+after[p+1]+after[p+2]>int(before[p])+before[p+1]+before[p+2]+12)++illuminated;
                require(illuminated>1000,"NASSP point emitters visibly illuminate cockpit surfaces");
                if(const auto path=std::getenv("ORBITER_COCKPIT_CAPTURE")) {
                    for(bool lit:{false,true}) {
                        std::ofstream image(std::string(path)+(lit?".floods-on.ppm":".floods-off.ppm"),std::ios::binary);
                        image<<"P6\n"<<width<<' '<<height<<"\n255\n";
                        const auto &pixels=lit?after:unlit;
                        for(size_t pixel=0;pixel<pixels.size();pixel+=4)image.write(reinterpret_cast<const char *>(pixels.data()+pixel),3);
                        require(bool(image),"write requested floodlight capture");
                    }
                }
                require(outputMesh()==materialMesh,"emitter updates preserve mesh topology");
                for(auto &[emitter,intensity]:emitters)emitter->SetIntensity(0);
                host.RenderNativeFrame();
                require(read(after.data(),after.size(),width*4) && after==unlit,"switching floods off removes their light");
                for(auto &[emitter,intensity]:emitters)emitter->SetIntensity(intensity);
                host.RenderNativeFrame();
                require(read(before.data(),before.size(),width*4) && before==materialBaseline,"restore original floodlight intensities");
                struct SavedLight {DEVMESHHANDLE mesh;DWORD index;oapi::FVECTOR4 value;};
                std::vector<SavedLight> savedLights;
                const oapi::FVECTOR4 redLight{2.0f,0.0f,0.0f,1.0f};
                for(UINT i=0;i<vesselInterface->GetMeshCount();++i) {
                    auto owned=static_cast<Mesh *>(graphics->clbkGetMesh(visual,i));
                    if(!owned)continue;
                    auto handle=reinterpret_cast<DEVMESHHANDLE>(owned);
                    for(DWORD j=0;j<owned->nMaterial();++j) {
                        oapi::FVECTOR4 value{};
                        require(oapiMeshMaterialEx(handle,j,Light,&value)==0,"read light through SDK");
                        savedLights.push_back({handle,j,value});
                        require(oapiSetMaterialEx(handle,j,Light,&redLight)==0,"set cockpit light through SDK");
                    }
                }
                host.RenderNativeFrame();
                require(read(after.data(),after.size(),width*4),"read relit cockpit");
                size_t relit=0;
                for(size_t p=0;p<before.size();p+=4)
                    if(int(after[p])-int(before[p])>32)++relit;
                require(relit>1000,"material-only update visibly lights cockpit");
                require(outputMesh()==materialMesh,"material edits preserve mesh topology");
                for(const auto &entry:savedLights)
                    require(oapiSetMaterialEx(entry.mesh,entry.index,Light,&entry.value)==0,"restore cockpit lighting");
                host.RenderNativeFrame();
                require(read(before.data(),before.size(),width*4),"restored cockpit before animation");
                require(before==materialBaseline,"material restoration preserves other vertex data");
                for(const auto &[id,mask]:drawMasks)areas.areas.at(id).draw=mask;
                if(powered) {
                    auto pressKey=[&](int id) {
                        const auto &key=areas.areas.at(id);
                        VECTOR3 point=key.points[0];
                        if(key.shape==2) {point=_V(0,0,0);for(const auto &p:key.points)point+=p*0.25;}
                        const auto eye=g_camera->CockpitPos();
                        const Vector ray(point.x-eye.x,point.y-eye.y,point.z-eye.z);
                        int x,y,ww,wh;
                        require(g_camera->Direction2Viewport(mul(vessel->GRot(),ray),x,y),"DSKY key in viewport");
                        SDL_GetWindowSize(window.Handle(),&ww,&wh);
                        SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
                        event.button.button=SDL_BUTTON_LEFT;event.button.down=true;
                        event.button.x=float(x)*ww/host.ViewW();event.button.y=float(y)*wh/host.ViewH();
                        require(SDL_PushEvent(&event) && window.Poll(),"dispatch DSKY key down");
                        require(areas.capture[0] && areas.capture[0]->id==id,"native picking selects DSKY key");
                        for(int i=0;i<30;++i)host.StepSimulation(1.0/60.0);
                        event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;
                        require(SDL_PushEvent(&event) && window.Poll(),"dispatch DSKY key up");
                        for(int i=0;i<30;++i)host.StepSimulation(1.0/60.0);
                    };
                    pressKey(4801);pressKey(4808);pressKey(4810); // VERB 3 5
                    std::ostringstream state;vessel->Write(state);
                    const auto begin=state.str().find("DSKY_BEGIN");
                    const auto end=state.str().find("DSKY_END",begin);
                    const auto display=state.str().substr(begin,end-begin);
                    std::cout<<display;
                    require(display.find("VERB 35")!=std::string::npos,"running AGC echoes clicked VERB 35");
                    host.RenderNativeFrame();
                    require(readSurface(dsky.surface,mapped.data(),mapped.size(),dw*4),"DSKY key response pixels");
                    require(mapped!=instrument,"AGC keyboard response updates DSKY texture");
                    pressKey(4818); // ENTER: execute the AGC display test.
                    std::ostringstream lampState;vessel->Write(lampState);
                    const auto lampBegin=lampState.str().find("DSKY_BEGIN");
                    const auto lampEnd=lampState.str().find("DSKY_END",lampBegin);
                    const auto lampDisplay=lampState.str().substr(lampBegin,lampEnd-lampBegin);
                    std::cout<<lampDisplay;
                    require(lampDisplay.find("PROG 88")!=std::string::npos && lampDisplay.find("VERB 88")!=std::string::npos && lampDisplay.find("NOUN 88")!=std::string::npos,"AGC executes clicked display test");
                    host.RenderNativeFrame();
                    std::vector<BYTE> lampPixels(mapped.size());
                    require(readSurface(dsky.surface,lampPixels.data(),lampPixels.size(),dw*4),"DSKY lamp test pixels");
                    require(lampPixels!=mapped,"AGC display test changes cockpit texture");
                    require(read(before.data(),before.size(),width*4),"cockpit before shade test");
                }
                const auto initialEquipment=equipment();
                // NASSP resource.h: DSKY glare shade, a persistent crew-equipment bit.
                const auto &shade=areas.areas.at(3217);
                VECTOR3 center{};for(const auto &point:shade.points)center+=point*0.25;
                const auto eye=g_camera->CockpitPos();
                const Vector local(center.x-eye.x,center.y-eye.y,center.z-eye.z);
                int clickX,clickY;
                require(g_camera->Direction2Viewport(mul(vessel->GRot(),local),clickX,clickY),"shade is visible");
                int windowW,windowH;SDL_GetWindowSize(window.Handle(),&windowW,&windowH);
                SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;click.button.button=SDL_BUTTON_LEFT;click.button.down=true;
                click.button.x=float(clickX)*windowW/host.ViewW();click.button.y=float(clickY)*windowH/host.ViewH();
                require(SDL_PushEvent(&click),"queue shade click");require(window.Poll(),"dispatch shade click");
                require(areas.capture[0] && areas.capture[0]->id==3217,"pick real NASSP shade");
                click.type=SDL_EVENT_MOUSE_BUTTON_UP;click.button.down=false;click.button.x=-100;click.button.y=-100;
                require(SDL_PushEvent(&click),"queue shade release");require(window.Poll(),"dispatch shade release");
                require(!areas.capture[0] && !areas.held[0],"release outside control clears capture");
                for(int i=0;i<150;++i)host.StepSimulation(1.0/60.0);
                require(((equipment()^initialEquipment)&16)!=0,"SDL click changes NASSP saved glare-shade state");
                oapiCameraGlobalDir(&beforeDirection);
                host.RenderNativeFrame();
                require(read(after.data(),after.size(),width*4),"animated cockpit after click");
                size_t changed=0;for(size_t p=0;p<before.size();p+=4) {
                    int delta=0;for(int k=0;k<3;++k)delta+=std::abs(int(before[p+k])-int(after[p+k]));
                    if(delta>64)++changed;
                }
                require(changed>1000,"NASSP animation visibly changes cockpit geometry");
                before=after;
                SDL_Event button{};button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;button.button.button=SDL_BUTTON_RIGHT;button.button.down=true;
                require(SDL_PushEvent(&button),"queue right mouse down");
                SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.xrel=120;
                require(SDL_PushEvent(&motion),"queue mouse look");
                button.type=SDL_EVENT_MOUSE_BUTTON_UP;button.button.down=false;
                require(SDL_PushEvent(&button),"queue right mouse up");
            }
            const unsigned frames=argc>=4?std::stoul(argv[3]):8;
            for(unsigned i=0;i<frames;++i) {require(window.Poll(),"window closed");if (live) host.StepSimulation(1.0/60.0);host.RenderNativeFrame();SDL_Delay(16);}
            if (live) {
                require(read(after.data(),after.size(),host.ViewW()*4),"mouse look readback");
                require(before!=after,"SDL mouse look changes rendered cockpit");
                VECTOR3 direction{};oapiCameraGlobalDir(&direction);
                require(length(direction-beforeDirection)>0.01,"SDL motion rotates simulation camera");
                std::cout<<"Live Saturn5 cockpit, VC mode and SDL mouse-look rendering verified\n";
                using ReadSurface=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
                auto readSurface=reinterpret_cast<ReadSurface>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));
                const auto oldArea=orbiter::NativeCockpitState().areas.at(4942);
                const DWORD w=oldArea.rect.right-oldArea.rect.left,h=oldArea.rect.bottom-oldArea.rect.top;
                std::vector<BYTE> original(size_t(w)*h*4),restored(original.size());
                require(readSurface(oldArea.background,original.data(),original.size(),w*4),"read pristine background before reentry");
                require(host.LoadNativeVirtualCockpit(),"reenter NASSP virtual cockpit");
                const auto &newArea=orbiter::NativeCockpitState().areas.at(4942);
                require(readSurface(newArea.background,restored.data(),restored.size(),w*4),"read reentry background");
                require(restored==original,"cockpit reentry preserves pristine background");
                host.RenderNativeFrame();
                const auto commanderEye=g_camera->CockpitPos();
                auto changeSeat=[&](SDL_Scancode direction) {
                    for(const auto key:{SDL_SCANCODE_LCTRL,direction}) {
                        SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=key;e.key.down=true;
                        require(SDL_PushEvent(&e),"queue cockpit navigation key down");
                    }
                    for(const auto key:{direction,SDL_SCANCODE_LCTRL}) {
                        SDL_Event e{};e.type=SDL_EVENT_KEY_UP;e.key.scancode=key;e.key.down=false;
                        require(SDL_PushEvent(&e),"queue cockpit navigation key up");
                    }
                    require(window.Poll(),"dispatch cockpit navigation");host.RenderNativeFrame();
                };
                require(orbiter::NativeCockpitState().neighbours[1]==2,"NASSP commander seat registers center neighbour");
                changeSeat(SDL_SCANCODE_RIGHT);
                require(orbiter::NativeCockpitState().cockpitId==2,"Ctrl-right selects center seat");
                require((g_camera->CockpitPos()-commanderEye).length()>0.1,"seat change moves actual cockpit camera");
                changeSeat(SDL_SCANCODE_LEFT);
                require(orbiter::NativeCockpitState().cockpitId==0 && (g_camera->CockpitPos()-commanderEye).length()<1e-9,"Ctrl-left returns to commander seat");
                changeSeat(SDL_SCANCODE_LEFT);
                require(orbiter::NativeCockpitState().cockpitId==0,"missing neighbour keeps current cockpit");
                if(powered) {
                    const bool backgroundEnabled=host.Cfg()->CfgVisualPrm.bUseBgImage;
                    const bool starImageEnabled=host.Cfg()->CfgVisualPrm.bUseStarImage;
                    host.Cfg()->CfgVisualPrm.bUseStarImage=false;
                    host.Cfg()->CfgVisualPrm.bUseBgImage=false;
                    auto vessel=oapiGetVesselInterface(oapiGetFocusObject());
                    std::vector<WORD> visibility;
                    for(UINT i=0;i<vessel->GetMeshCount();++i) {visibility.push_back(vessel->GetMeshVisibilityMode(i));vessel->SetMeshVisibilityMode(i,MESHVIS_NEVER);}
                    char earthName[]="Earth";VECTOR3 earth{},local{};
                    oapiGetGlobalPos(oapiGetGbodyByName(earthName),&earth);vessel->Global2Local(earth,local);
                    const auto eye=g_camera->CockpitPos();local-=_V(eye.x,eye.y,eye.z);normalise(local);
                    vessel->SetCameraDefaultDirection(local);g_camera->ResetCockpitDir(false);g_camera->Update();
                    host.RenderNativeFrame();
                    require(read(after.data(),after.size(),host.ViewW()*4),"planet scene readback");
                    const auto center=(size_t(host.ViewH()/2)*host.ViewW()+host.ViewW()/2)*4;
                    std::cout<<"Earth centre RGB: "<<int(after[center])<<", "<<int(after[center+1])<<", "<<int(after[center+2])<<std::endl;
                    // Downloaded terrain imagery may place land at this pixel;
                    // verify visibility and the opposite-camera check below,
                    // rather than requiring the old untextured blue sphere.
                    require(int(after[center])+after[center+1]+after[center+2]>6,"simulation Earth appears along its camera-relative direction");
                    if(const auto path=std::getenv("ORBITER_COCKPIT_CAPTURE")) {
                        std::ofstream image(std::string(path)+".earth.ppm",std::ios::binary);
                        image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
                        for(size_t pixel=0;pixel<after.size();pixel+=4)image.write(reinterpret_cast<const char *>(after.data()+pixel),3);
                        require(bool(image),"write Earth capture");
                    }
                    if(std::getenv("ORBITER_CHECK_CLOUDS")) {
                        const auto cloudy=after;
                        const bool enabled=host.Cfg()->CfgVisualPrm.bClouds;
                        require(enabled,"cloud verification starts with clouds enabled");
                        host.Cfg()->CfgVisualPrm.bClouds=false;host.RenderNativeFrame();
                        require(read(after.data(),after.size(),host.ViewW()*4),"cloud-free Earth readback");
                        size_t changed=0;
                        for(size_t pixel=0;pixel<after.size();pixel+=4)
                            if(std::abs(int(after[pixel])-int(cloudy[pixel]))+std::abs(int(after[pixel+1])-int(cloudy[pixel+1]))+std::abs(int(after[pixel+2])-int(cloudy[pixel+2]))>6)++changed;
                        require(changed>10,"downloaded clouds change visible Earth pixels");
                        std::cout<<"Cloud layer changes "<<changed<<" Earth pixels"<<std::endl;
                        host.Cfg()->CfgVisualPrm.bClouds=enabled;host.RenderNativeFrame();
                    }
                    vessel->SetCameraDefaultDirection(-local);g_camera->ResetCockpitDir(false);g_camera->Update();host.RenderNativeFrame();
                    require(read(after.data(),after.size(),host.ViewW()*4),"look away from planet");
                    require(after[center]==0 && after[center+1]==0 && after[center+2]==0,"planet leaves view when looking away");
                    const auto emptySky=after;
                    const bool dotsEnabled=host.Cfg()->CfgVisualPrm.bUseStarDots;
                    require(dotsEnabled,"catalog toggle test begins with stars enabled");
                    host.Cfg()->CfgVisualPrm.bUseStarDots=false;host.RenderNativeFrame();
                    require(read(after.data(),after.size(),host.ViewW()*4),"catalog-disabled sky readback");
                    size_t removed=0;for(size_t i=0;i<after.size();i+=4)if(after[i]!=emptySky[i] || after[i+1]!=emptySky[i+1] || after[i+2]!=emptySky[i+2])++removed;
                    require(removed>100,"disabling catalog dots changes actual sky pixels");
                    host.Cfg()->CfgVisualPrm.bUseStarDots=dotsEnabled;host.RenderNativeFrame();
                    require(read(after.data(),after.size(),host.ViewW()*4),"catalog-restored sky readback");
                    require(after==emptySky,"restoring catalog dots restores the exact sky frame");
                    std::cout<<"Catalog star toggle changes "<<removed<<" pixels"<<std::endl;

                    if(std::getenv("ORBITER_CHECK_STAR_IMAGE")) {
                        host.Cfg()->CfgVisualPrm.bUseStarImage=true;host.RenderNativeFrame();
                        require(read(after.data(),after.size(),host.ViewW()*4),"star-image-only readback");
                        size_t changed=0;for(size_t i=0;i<after.size();i+=4)if(after[i]!=emptySky[i] || after[i+1]!=emptySky[i+1] || after[i+2]!=emptySky[i+2])++changed;
                        require(changed>100,"downloaded star image contributes visible pixels independently of Milky Way");
                        std::cout<<"Star image changes "<<changed<<" pixels"<<std::endl;
                        if(const auto path=std::getenv("ORBITER_COCKPIT_CAPTURE")) {
                            std::ofstream image(std::string(path)+".stars.ppm",std::ios::binary);
                            image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
                            for(size_t i=0;i<after.size();i+=4)image.write(reinterpret_cast<const char *>(after.data()+i),3);
                            require(bool(image),"write star-image capture");
                        }
                    }

                    host.Cfg()->CfgVisualPrm.bUseBgImage=backgroundEnabled;
                    host.Cfg()->CfgVisualPrm.bUseStarImage=starImageEnabled;host.RenderNativeFrame();
                    if(std::getenv("ORBITER_CHECK_BACKGROUND")) {
                        require(read(after.data(),after.size(),host.ViewW()*4),"celestial image readback");
                        size_t changed=0;for(size_t i=0;i<after.size();i+=4)if(after[i]!=emptySky[i] || after[i+1]!=emptySky[i+1] || after[i+2]!=emptySky[i+2])++changed;
                        require(changed>100,"legacy background texture contributes visible pixels");
                        std::cout<<"Celestial background changes "<<changed<<" pixels"<<std::endl;
                        if(const auto path=std::getenv("ORBITER_COCKPIT_CAPTURE")) {
                            std::ofstream image(std::string(path)+".background.ppm",std::ios::binary);
                            image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
                            for(size_t i=0;i<after.size();i+=4)image.write(reinterpret_cast<const char *>(after.data()+i),3);
                            require(bool(image),"write celestial background capture");
                        }
                    }
                    for(UINT i=0;i<visibility.size();++i)vessel->SetMeshVisibilityMode(i,visibility[i]);
                    require(host.LoadNativeVirtualCockpit(),"restore cockpit after planet check");host.RenderNativeFrame();
                }
                auto flashlight=[&]() -> const LightEmitter * {
                    auto vessel=oapiGetVesselInterface(oapiGetFocusObject());
                    for(DWORD i=0;i<vessel->LightEmitterCount();++i) {
                        auto light=vessel->GetLightEmitter(i);
                        if(light->GetType()==LightEmitter::LT_SPOT && light->GetVisibility()==LightEmitter::VIS_COCKPIT)return light;
                    }
                    throw std::runtime_error("NASSP flashlight emitter missing");
                };
                auto toggleFlashlight=[&]() {
                    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_F;e.key.down=true;
                    require(SDL_PushEvent(&e),"queue flashlight F key");
                    e.type=SDL_EVENT_KEY_UP;e.key.down=false;require(SDL_PushEvent(&e),"queue flashlight key release");
                    require(window.Poll(),"dispatch flashlight key");
                };
                require(!flashlight()->IsActive(),"flashlight initially off");
                toggleFlashlight();require(flashlight()->IsActive(),"F activates NASSP flashlight");
                auto checkBeam=[&]() {
                    auto focusInterface=oapiGetFocusInterface();
                    // A fully illuminated diffuse texture cannot become brighter in this
                    // bounded lighting path. Isolate the flashlight from the floodlights.
                    std::vector<std::pair<LightEmitter *,bool>> floods;
                    for(DWORD i=0;i<focusInterface->LightEmitterCount();++i) {
                        auto light=const_cast<LightEmitter *>(focusInterface->GetLightEmitter(i));
                        if(light->GetType()==LightEmitter::LT_POINT) {floods.emplace_back(light,light->IsActive());light->Activate(false);}
                    }
                    require(flashlight()->IsCameraRelative(),"flashlight uses render-time camera pose");
                    require(length(flashlight()->GetPosition())==0 && length(flashlight()->GetDirection()-_V(0,0,1))==0,"flashlight originates at camera and points forward");
                    host.RenderNativeFrame();
                    require(read(before.data(),before.size(),host.ViewW()*4),"lit flashlight image");
                    toggleFlashlight();require(!flashlight()->IsActive(),"F deactivates NASSP flashlight");
                    host.RenderNativeFrame();require(read(after.data(),after.size(),host.ViewW()*4),"unlit flashlight image");
                    size_t beamPixels=0;
                    for(size_t p=0;p<before.size();p+=4)
                        if(int(before[p])+before[p+1]+before[p+2]>int(after[p])+after[p+1]+after[p+2]+12)++beamPixels;
                    require(beamPixels>1000,"NASSP flashlight visibly lights cockpit through SDL key input");
                    toggleFlashlight();require(flashlight()->IsActive(),"F reactivates flashlight");
                    for(auto [light,active]:floods)light->Activate(active);
                };
                oapiSetPause(true);host.StepSimulation(1.0/60.0);
                require(oapiGetPause(),"pause live cockpit for inspection");
                const double pausedTime=oapiGetSimTime();checkBeam();
                // Camera movement and seat navigation must work without another simulation tick.
                g_camera->SetCockpitDir(0.2,-0.1);g_camera->Update();checkBeam();
                require(host.LoadNativeVirtualCockpit(2),"move flashlight to center seat");
                require(flashlight()->IsActive(),"flashlight activation survives seat change");checkBeam();
                toggleFlashlight();require(host.LoadNativeVirtualCockpit(0),"restore commander seat after flashlight check");
                host.RenderNativeFrame();
                require(oapiGetSimTime()==pausedTime,"cockpit interaction and rendering do not advance paused simulation");
                oapiSetPause(false);host.StepSimulation(1.0/60.0);
                require(!oapiGetPause() && oapiGetSimTime()>pausedTime,"resume live cockpit after inspection");
                std::cout<<"NASSP flashlight key, visible beam, look tracking and seat changes verified\n";
                using StarCount=size_t (*)();
                auto starCount=reinterpret_cast<StarCount>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_star_count"));
                require(starCount && starCount()==41057,"live scene loads Orbiter star catalog");
                if(const auto path=std::getenv("ORBITER_COCKPIT_CAPTURE")) {
                    g_camera->SetCockpitDir(0,1.1);g_camera->Update();host.RenderNativeFrame();
                    require(read(after.data(),after.size(),host.ViewW()*4),"window-facing cockpit capture");
                    std::ofstream image(std::string(path)+".sky.ppm",std::ios::binary);
                    image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
                    for(size_t pixel=0;pixel<after.size();pixel+=4)image.write(reinterpret_cast<const char *>(after.data()+pixel),3);
                    require(bool(image),"write requested sky capture");
                }
                auto focus=reinterpret_cast<Vessel *>(oapiGetFocusObject());auto stale=focus->GetVishandle();
                static_cast<oapi::Module *>(host.GetGraphicsClient())->clbkDeleteVessel(oapiGetFocusObject());
                require(!focus->GetVishandle(),"visual destroyed callback clears core handle");
                require(!host.GetGraphicsClient()->clbkGetMesh(stale,0),"released visual rejected");
            }
        }
        std::cout<<"Real NASSP 3D cockpit geometry rendered from commander seat\n";
        return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
