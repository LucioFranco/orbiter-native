#include "Orbiter.h"
#include "Platform/DynamicLibrary.h"
#include "PanelSDK/PanelSDK.h"
#include "PanelSDK/Internals/Esystems.h"
#include "FDAI.h"
#include "NasspBitmapResources.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
struct Supply: e_object {double volts;explicit Supply(double v):volts(v){} double Voltage()override{return volts;}};
int main(int argc,char **argv) {
    try {
        require(argc==3,"graphics module and staged runtime required");
        const auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);Orbiter host;
        auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());auto gc=host.GetGraphicsClient();
        require(module && gc && gc->clbkInitialise(),"initialize native FDAI graphics");
        using Read=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));require(read,"read rendered FDAI");
        std::vector<std::vector<BYTE>> captures;
        for(bool lm:{false,true}) {
            Supply dc(28),ac(115);FDAI instrument;
            if(lm){instrument.SetLMmode();instrument.WireTo(&dc);}else instrument.WireTo(&dc,&ac);
            std::vector<SURFHANDLE> resources;
            auto load=[&](unsigned id){
                const auto path=lm?nassp::resources::FindBitmap(nassp::resources::lm,id):nassp::resources::FindBitmap(nassp::resources::csm,id);
                auto surf=oapiLoadSurfaceEx(path,OAPISURFACE_TEXTURE,true);require(surf,"load original instrument bitmap");
                oapiSetSurfaceColourKey(surf,0xff00ff);resources.push_back(surf);return surf;
            };
            auto frame=lm?nullptr:load(255),roll=load(lm?10006:532),off=load(793),needles=load(lm?327:299);
            auto surface=oapiCreateSurface(245,245);require(surface,"create instrument target");
            auto render=[&](VECTOR3 rates,VECTOR3 errors){
                oapiColourFill(surface,0x202020);
                instrument.PaintMe(rates,errors,surface,frame,roll,off,needles,1);
                std::vector<BYTE> pixels(245*245*4);require(read(surface,pixels.data(),pixels.size(),245*4),"read composed FDAI pixels");return pixels;
            };
            auto zero=render(_V(0,0,0),_V(0,0,0));
            auto needleMoved=render(_V(.5,-.5,.8),_V(15,-12,10));require(zero!=needleMoved,"rate and error needles move while powered");
            instrument.SetAttitude(_V(.4,.2,-.3));instrument.Timestep(10,10);
            auto tilted=render(_V(.5,-.5,.8),_V(15,-12,10));require(tilted!=needleMoved,"CPU ball responds to original attitude mechanism");
            captures.push_back(zero);captures.push_back(tilted);
            dc.volts=0;instrument.SetAttitude(_V(1,1,1));instrument.Timestep(20,10);
            auto unpowered=render(_V(-1,1,-1),_V(-30,30,-30));
            if(lm)require(unpowered!=tilted,"LM off flag appears without power");else require(unpowered==tilted,"CSM ball and needles retain their last powered state");
            captures.push_back(unpowered);
            for(auto resource:resources)oapiDestroySurface(resource);oapiDestroySurface(surface);
        }
        if(const char *path=std::getenv("ORBITER_FDAI_CAPTURE")) {
            std::ofstream out(path,std::ios::binary);out<<"P6\n735 490\n255\n";
            for(unsigned y=0;y<490;++y)for(unsigned x=0;x<735;++x) {
                const auto &pixels=captures[(y/245)*3+x/245];out.write(reinterpret_cast<const char *>(&pixels[((y%245)*245+x%245)*4]),3);
            }
            require(bool(out),"save FDAI comparison");
        }
        std::cout<<"Original CSM/LM 2D FDAI drawing, CPU upload, needle motion and power loss verified\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
