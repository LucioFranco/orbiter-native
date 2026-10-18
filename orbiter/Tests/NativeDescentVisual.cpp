#include "Orbiter.h"
#include "Vessel.h"
#include "NativeCockpit.h"
#include "NativeXPointer.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv){try {
    require(argc==3,"graphics plugin and verified descent runtime required");
    const auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);
    Orbiter host;auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());
    require(module && host.GetGraphicsClient()->clbkInitialise(),"initialize descent checkpoint graphics");
    host.Launch("NativeDescentContinued");
    auto eagle=oapiGetVesselByName(const_cast<char *>("Eagle"));require(eagle,"landed Eagle retained");
    oapiSetFocusObject(eagle);auto sdk=oapiGetVesselInterface(eagle);auto vessel=reinterpret_cast<Vessel *>(eagle);
    require(host.LoadNativeVirtualCockpit(),"load landed Eagle cockpit");orbiter::platform::SDLWindow window(host,false,true);
    for(int i=0;i<30;++i)host.StepSimulation(1.0/60.0);
    require(sdk->GroundContact(),"verified descent checkpoint remains in physical ground contact");
    std::ostringstream saved;vessel->Write(saved);auto state=saved.str();
    const auto a=state.find("DSKY_BEGIN"),b=state.find("DSKY_END",a);
    require(a!=std::string::npos && b!=std::string::npos && state.substr(a,b-a).find("PROG 65")!=std::string::npos,"actual LGC P65 display retained after fresh resume");
    for(DWORD i=0;i<sdk->GetThrusterCount();++i){auto t=sdk->GetThrusterHandleByIndex(i);if(sdk->GetThrusterMax(t)>40000)require(sdk->GetThrusterLevel(t)<.001,"DPS remains stopped during visual validation");}
    using Read=bool (*)(BYTE *,size_t,DWORD);using SurfaceRead=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
    auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));
    auto readSurface=reinterpret_cast<SurfaceRead>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));require(read&&readSurface,"read descent cockpit and atlas");
    verifyNativeXPointer(host,vessel,read);
    require(host.LoadNativeVirtualCockpit(0),"restore commander seat");host.RenderNativeFrame();
    std::vector<BYTE> pixels;
    if(const char *path=std::getenv("ORBITER_DESCENT_CAPTURE")){
        pixels.resize(size_t(host.ViewW())*host.ViewH()*4);require(read(pixels.data(),pixels.size(),host.ViewW()*4),"read actual P65 landed cockpit capture");
        std::ofstream out(path,std::ios::binary);out<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
        for(size_t i=0;i<pixels.size();i+=4)out.write(reinterpret_cast<const char *>(pixels.data()+i),3);require(bool(out),"save landed cockpit capture");
    }
    require(host.LoadNativeVirtualCockpit(3),"select original DSKY close-up");host.RenderNativeFrame();
    const auto &display=orbiter::NativeCockpitState().areas.at(3425);
    const DWORD w=display.rect.right-display.rect.left,h=display.rect.bottom-display.rect.top;
    pixels.resize(size_t(w)*h*4);
    require(display.surface&&display.target&&display.redraws&&readSurface(display.surface,pixels.data(),pixels.size(),w*4),"actual P65 DSKY redraw reaches texture");
    // Check a real LGC display command rather than assuming a particular RGB
    // color for the original dimmed bitmap. This process never saves over P65.
    for(int area:{3401,3408,3410,3418}) {
        Vector point(0.5,0.5,0);require(vessel->VCMouseEvent(area,PANEL_MOUSE_LBDOWN,point),"landed DSKY V35 key accepted");
        for(int i=0;i<30;++i)host.StepSimulation(1.0/60.0);
        vessel->VCMouseEvent(area,PANEL_MOUSE_LBUP,point);for(int i=0;i<30;++i)host.StepSimulation(1.0/60.0);
    }
    std::ostringstream lampState;vessel->Write(lampState);const auto lamp=lampState.str();
    const auto begin=lamp.find("DSKY_BEGIN"),end=lamp.find("DSKY_END",begin);
    require(begin!=std::string::npos && end!=std::string::npos && lamp.substr(begin,end-begin).find("PROG 88")!=std::string::npos,"original LGC executes landed display test");
    host.RenderNativeFrame();std::vector<BYTE> after(pixels.size());
    require(readSurface(display.surface,after.data(),after.size(),w*4) && after!=pixels,"landed LGC display command changes actual cockpit atlas");
    std::cout<<"Guided-descent checkpoint: original P65 DSKY, both X-pointers, landed state and stopped DPS verified\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
