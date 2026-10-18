#include "Orbiter.h"
#include "Vessel.h"
#include "Mesh.h"
#include "Camera.h"
#include "NativeCockpit.h"
#include "NativeMFD.h"
#include "Mfd.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <set>
#include <vector>
extern Camera *g_camera;
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv){try {
    const bool resume=argc==4 && std::string(argv[3])=="--resume";
    require(argc==3 || resume,"graphics plugin and original insertion runtime required");
    const auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);
    Orbiter host;
    host.Cfg()->Load("Orbiter.cfg");
    require(!host.Cfg()->CfgLogicPrm.bStartPaused,"manual MCC protocol requires StartPaused=FALSE before Launch");
    auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());
    auto gc=host.GetGraphicsClient();require(module&&gc&&gc->clbkInitialise(),"initialize MCC graphics");
    host.Launch(resume?"NativeMCCSaved":"Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion");
    auto mcc=oapiGetVesselByName(const_cast<char *>("MCC"));require(mcc,"original MCC vessel loaded");
    for(DWORD i=0;oapiGetFocusObject()!=mcc && i<oapiGetVesselCount();++i)host.CycleNativeFocus();
    require(oapiGetFocusObject()==mcc&&host.LoadNativeVirtualCockpit(),"select original MCC cockpit");
    orbiter::platform::SDLWindow window(host,false,true);
    using Read=bool (*)(BYTE *,size_t,DWORD);
    auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));require(read,"read MCC scene");
    auto frame=[&](){host.RenderNativeFrame();std::vector<BYTE> p(size_t(host.ViewW())*host.ViewH()*4);require(read(p.data(),p.size(),host.ViewW()*4),"capture MCC");return p;};
    const auto &state=orbiter::NativeCockpitState();require(state.mfdScreens.size()==4,"MCC registers all four screens");
    for(int id=0;id<4;++id) {
        if(!resume)oapiOpenMFD(MFD_SURFACE,id);
        require(oapiGetMFDMode(id)==MFD_SURFACE,"all four MCC modes opened or restored");
        if(!resume) {const unsigned keys[]={OAPI_KEY_G,OAPI_KEY_T,OAPI_KEY_I,OAPI_KEY_O};oapiSendMFDKey(id,keys[id]);}
    }
    auto settings=[&](int id){std::ostringstream out;orbiter::NativeMFDGet(id)->Write(out);return out.str();};
    auto checkSettings=[&](){
        std::set<SURFHANDLE> surfaces;
        for(int id=0;id<4;++id) {
            require(oapiGetMFDMode(id)==MFD_SURFACE,"MCC slots retain independent modes");
            require(settings(id).find("  SPDMODE "+std::to_string(id+1)+"\n")!=std::string::npos,"four distinct MCC settings retained");
            surfaces.insert(orbiter::NativeMFDGet(id)->Surface());
        }
        require(surfaces.size()==4,"four MCC slots have distinct instrument surfaces");
    };
    checkSettings();frame();
    using ReadSurface=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
    auto readSurface=reinterpret_cast<ReadSurface>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));
    require(readSurface,"read independent MCC instruments");
    auto instrumentImage=[&](int id){auto instrument=orbiter::NativeMFDGet(id);instrument->Update(1.0);std::vector<BYTE> pixels(512*512*4);require(readSurface(instrument->Surface(),pixels.data(),pixels.size(),512*4),"read MCC instrument pixels");return pixels;};
    std::vector<std::vector<BYTE>> distinct;
    for(int id=0;id<4;++id){distinct.push_back(instrumentImage(id));for(int previous=0;previous<id;++previous)require(distinct[id]!=distinct[previous],"four MCC settings produce distinct pixels");}
    std::cout<<"MCC distinct slots: four surfaces, SPDMODE 1/2/3/4 and four different images verified\n";
    auto sdk=oapiGetVesselInterface(mcc);auto vessel=reinterpret_cast<Vessel *>(mcc);
    for(int id=0;id<4;++id){
        const auto spec=state.mfdScreens.at(id);
        auto source=static_cast<Mesh *>(sdk->GetMeshTemplate(spec.nmesh));
        auto owned=static_cast<Mesh *>(gc->clbkGetMesh(vessel->GetVishandle(),spec.nmesh));
        require(source&&owned&&source!=owned,"MCC screen uses independent visual mesh");
        const DWORD original=source->GetGroupUsrFlag(spec.ngroup),flags=owned->GetGroupUsrFlag(spec.ngroup);
        // Aim at each original console screen so all four bindings are checked at useful resolution.
        const auto group=source->GetGroup(spec.ngroup);VECTOR3 center{};
        for(DWORD v=0;v<group->nVtx;++v)center+=_V(group->Vtx[v].x,group->Vtx[v].y,group->Vtx[v].z);
        center/=group->nVtx;VECTOR3 eye{};sdk->GetCameraOffset(eye);sdk->SetCameraDefaultDirection(unit(center-eye));g_camera->ResetCockpitDir(false);g_camera->Update();
        const auto on=frame();
        GROUPEDITSPEC edit{};edit.flags=GRPEDIT_SETUSERFLAG;edit.UsrFlag=flags|2;
        require(oapiEditMeshGroup(reinterpret_cast<DEVMESHHANDLE>(owned),spec.ngroup,&edit)==0,"hide MCC screen");const auto hidden=frame();
        edit.UsrFlag=flags;require(oapiEditMeshGroup(reinterpret_cast<DEVMESHHANDLE>(owned),spec.ngroup,&edit)==0,"restore MCC screen");
        require(frame()==on,"MCC screen restore reproduces scene");
        oapiToggleMFD_on(id);const auto off=frame();size_t changed=0;
        for(size_t p=0;p<on.size();p+=4)if(!std::equal(on.begin()+p,on.begin()+p+3,hidden.begin()+p)&&!std::equal(on.begin()+p,on.begin()+p+3,off.begin()+p))++changed;
        std::cout<<"MCC screen "<<id<<" powered display pixels="<<changed<<'\n';
        require(changed>100,"actual MCC screen displays its powered MFD");
        oapiToggleMFD_on(id);require(frame()==on,"MCC screen power-on restores display");
        require(source->GetGroupUsrFlag(spec.ngroup)==original,"original MCC mesh stays intact");
        std::vector<std::string> otherSettings;
        std::vector<std::vector<BYTE>> otherImages;
        for(int other=0;other<4;++other){otherSettings.push_back(settings(other));otherImages.push_back(instrumentImage(other));}
        oapiOpenMFD(MFD_ORBIT,id);const auto alternate=frame();size_t modePixels=0;
        for(size_t p=0;p<on.size();p+=4)if(!std::equal(on.begin()+p,on.begin()+p+3,hidden.begin()+p)&&!std::equal(on.begin()+p,on.begin()+p+3,alternate.begin()+p))++modePixels;
        require(modePixels>100,"changing one MCC mode changes its own physical screen");
        for(int other=0;other<4;++other)if(other!=id)require(settings(other)==otherSettings[other] && instrumentImage(other)==otherImages[other],"one MCC mode change leaves other slots' state and pixels unchanged");
        oapiOpenMFD(MFD_SURFACE,id);checkSettings();require(frame()==on,"MCC mode restore reproduces its original screen");
        std::cout<<"MCC slot "<<id<<" isolated mode-change pixels="<<modePixels<<'\n';
    }
    require(host.LoadNativeVirtualCockpit(0),"restore MCC view");
    // Switching vessels and returning must retain all four independent instruments.
    auto other=oapiGetVesselByName(const_cast<char *>("Columbia"));require(other,"original Columbia remains loaded");
    oapiSetFocusObject(other);require(host.LoadNativeVirtualCockpit(),"return to spacecraft cockpit");frame();
    const VCMFDSPEC *spec=nullptr;require(!gc->GetVCMFDSurface(2,&spec) && !spec,"MCC screen binding does not leak to Columbia");
    oapiSetFocusObject(mcc);require(host.LoadNativeVirtualCockpit(),"return to MCC default cockpit");
    for(int id=0;id<4;++id)require(oapiGetMFDMode(id)==MFD_SURFACE,"MCC mode retained across focus change");
    checkSettings();auto capture=frame();require(host.SaveScenario("NativeMCCSaved","Native MCC display validation",0),"save four MCC modes");
    if(const char *path=std::getenv("ORBITER_MCC_CAPTURE")){std::ofstream out(path,std::ios::binary);out<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";for(size_t i=0;i<capture.size();i+=4)out.write(reinterpret_cast<const char*>(capture.data()+i),3);require(bool(out),"save MCC capture");}
    // ImGui measures a newly opened auto-sized window on its first frame.
    // Settle layout before checking/capturing the actual panel.
    orbiter::NativeMFDToggle();frame();frame();capture=frame();
    auto panel=ImGui::FindWindowByName("Flight instruments (F2)");
    require(panel && panel->Active && !panel->Hidden,"four-display picker visible after layout");
    require(panel->Pos.y+panel->Size.y<=ImGui::GetIO().DisplaySize.y+1,"four-display picker fits laptop viewport");
    require(panel->ScrollMax.y>0,"extra MCC displays available through scrolling");
    auto saveUI=[&](const std::filesystem::path &path,const std::vector<BYTE> &pixels){
        std::ofstream out(path,std::ios::binary);out<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
        for(size_t i=0;i<pixels.size();i+=4)out.write(reinterpret_cast<const char *>(pixels.data()+i),3);
        require(bool(out),"save four-display mode picker");
    };
    if(const char *path=std::getenv("ORBITER_MCC_UI_CAPTURE"))saveUI(path,capture);
    const float x=panel->Pos.x+40,y=panel->Pos.y+80;
    SDL_WarpMouseInWindow(window.Handle(),x,y);require(window.Poll(),"hover MCC picker");frame();frame();
    SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.windowID=SDL_GetWindowID(window.Handle());
    wheel.wheel.mouse_x=x;wheel.wheel.mouse_y=y;wheel.wheel.y=-50;
    require(SDL_PushEvent(&wheel) && window.Poll(),"scroll MCC picker through SDL");frame();frame();capture=frame();
    require(panel->Scroll.y>0 && panel->Scroll.y>=panel->ScrollMax.y-1,"SDL scroll reaches displays three and four");
    if(const char *path=std::getenv("ORBITER_MCC_UI_CAPTURE")) {
        std::filesystem::path target(path);saveUI(target.parent_path()/(target.stem().string()+"-scrolled.ppm"),capture);
    }
    std::cout<<"MCC picker viewport fit and actual SDL scrolling to displays 3/4 verified\n";
    orbiter::NativeMFDToggle();
    std::cout<<"Original MCC cockpit and all four native MFD screen bindings verified\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
