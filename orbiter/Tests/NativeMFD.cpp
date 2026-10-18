#include "Orbiter.h"
#include "Vessel.h"
#include "NativeMFD.h"
#include "Mfd.h"
#include "Select.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include "imgui.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
extern Select *g_select;
static_assert(alignof(MFDBUTTONMENU)==alignof(const char *) && sizeof(MFDBUTTONMENU)%alignof(const char *)==0,"MFD menu pointers must remain aligned in arrays");
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
namespace {
int created=0,destroyed=0,updates=0;
class TestMFD final:public MFD2 {
public:
    TestMFD(DWORD w,DWORD h,VESSEL *v):MFD2(w,h,v) {++created;}
    ~TestMFD() override {++destroyed;}
    bool Update(oapi::Sketchpad *pad) override {++updates;Title(pad,"Native addon MFD");pad->SetPen(GetDefaultPen(0));pad->Line(10,60,200,120);return true;}
};
INT_PTR callback(UINT message,UINT mfd,WPARAM wparam,LPARAM lparam) {
    if(message==OAPI_MSG_MFD_OPENEDEX) {
        auto spec=reinterpret_cast<MFDMODEOPENSPEC *>(wparam);
        return reinterpret_cast<INT_PTR>(new TestMFD(spec->w,spec->h,reinterpret_cast<VESSEL *>(lparam)));
    }
    return 0;
}
}
int main(int argc,char **argv) {
    try {
        const bool resume=argc==4 && std::string(argv[3])=="--resume";
        require(argc==3 || resume,"graphics plugin and pair runtime required");
        auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);Orbiter host;
        auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());auto gc=host.GetGraphicsClient();
        require(module && gc && gc->clbkInitialise(),"initialize MFD graphics");host.Launch(resume?"NativeMFDSaved":"NasspPair");
        require(host.LoadNativeVirtualCockpit(),"load CSM cockpit");orbiter::platform::SDLWindow window(host,false,true);
        for(int i=0;i<60;++i)host.StepSimulation(1.0/60);
        const auto csm=oapiGetFocusObject();
        if(!resume) {oapiOpenMFD(MFD_ORBIT,0);oapiOpenMFD(MFD_SURFACE,1);}
        require(oapiGetMFDMode(0)==MFD_ORBIT && oapiGetMFDMode(1)==MFD_SURFACE,"original Orbit and Surface instruments open through SDK");
        require(std::string(oapiMFDButtonLabel(0,0))=="REF","original button labels exposed");
        using Read=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));require(read,"read MFD surface");
        auto image=[&](int id) {auto instrument=orbiter::NativeMFDGet(id);instrument->Update(1);std::vector<BYTE> out(512*512*4);require(read(instrument->Surface(),out.data(),out.size(),512*4),"MFD readback");return out;};
        auto orbit=image(0),surface=image(1);size_t green=0,color=0;
        for(size_t i=0;i<orbit.size();i+=4) {green+=orbit[i+1]>100 && orbit[i]<80;color+=surface[i]>50 || surface[i+2]>50;}
        require(green>1000 && color>1000 && orbit!=surface,"orbit graph/text and surface horizon/tapes rendered by original instruments");
        if(resume) {
            std::ostringstream restored;orbiter::NativeMFDGet(0)->Write(restored);
            require(restored.str().find("  ALT\n")!=std::string::npos && restored.str().find("  REF Moon\n")!=std::string::npos,"fresh process restores Orbit settings");
            std::cout<<"Fresh-process Orbit/Surface state and rendering verified\n";return 0;
        }
        oapiToggleMFD_on(1);require(oapiGetMFDMode(1)==MFD_NONE,"power off Surface");
        oapiToggleMFD_on(1);require(oapiGetMFDMode(1)==MFD_SURFACE,"power restores previous mode");
        require(oapiProcessMFDButton(0,7,PANEL_MOUSE_LBDOWN),"distance mode button dispatch");
        require(image(0)!=orbit,"distance mode changes instrument pixels");
        std::ostringstream state;orbiter::NativeMFDGet(0)->Write(state);require(state.str().find("  ALT\n")!=std::string::npos,"radius/altitude setting changes original MFD state");
        require(oapiProcessMFDButton(0,0,PANEL_MOUSE_LBDOWN) && g_select->IsActive(),"reference button opens original selection dialog");
        char moon[]="Moon";require(g_select->cbEnter(g_select,0,moon,g_select->userdata),"reference selector accepts Moon");orbiter::NativeDialogsReset();
        require(host.CycleNativeFocus() && oapiGetFocusObject()!=csm,"switch to Eagle");
        oapiOpenMFD(MFD_SURFACE,0);require(oapiGetMFDMode(0)==MFD_SURFACE,"Eagle has independent MFD mode");
        require(host.CycleNativeFocus() && oapiGetFocusObject()==csm && oapiGetMFDMode(0)==MFD_ORBIT,"return to Columbia retains its instrument");
        SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=SDL_SCANCODE_F2;key.key.down=true;
        require(SDL_PushEvent(&key) && window.Poll(),"F2 opens instrument window");host.RenderNativeFrame();host.RenderNativeFrame();
        require(orbiter::NativeMFDVisible(),"native MFD window visible over cockpit");
        // Keep images alive when a mode changes after ImGui recorded this frame.
        gc->clbkImGuiNewFrame();ImGui::NewFrame();orbiter::NativeMFDDraw();oapiOpenMFD(MFD_SURFACE,0);
        ImGui::Render();gc->clbkImGuiRenderDrawData();orbiter::NativeMFDFrameEnd();
        require(oapiGetMFDMode(0)==MFD_SURFACE,"mode replacement during a recorded UI frame succeeds");
        MFDMODESPECEX custom{const_cast<char *>("Native test"),OAPI_KEY_J,nullptr,callback};
        const int mode=oapiRegisterMFDMode(custom);require(mode>BUILTIN_MFD_MODES,"register addon MFD2 mode");
        oapiOpenMFD(mode,0);image(0);require(created==1 && updates>0,"original MFD2 callback and Sketchpad update run");
        require(oapiUnregisterMFDMode(mode) && destroyed==1 && oapiGetMFDMode(0)==MFD_NONE,"unregistering an active addon closes its instance");
        oapiOpenMFD(MFD_ORBIT,0);
        std::ostringstream saved;orbiter::NativeMFDGet(0)->Write(saved);
        if(saved.str().find("  ALT\n")==std::string::npos)oapiSendMFDKey(0,OAPI_KEY_D);
        host.RenderNativeFrame();
        require(host.SaveScenario("NativeMFDSaved","Native instruments test",0),"save MFD modes and parameters");
        if(const char *path=std::getenv("ORBITER_MFD_CAPTURE")) {
            using ReadOutput=bool (*)(BYTE *,size_t,DWORD);auto readOutput=reinterpret_cast<ReadOutput>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));
            std::vector<BYTE> pixels(size_t(host.ViewW())*host.ViewH()*4);require(readOutput && readOutput(pixels.data(),pixels.size(),host.ViewW()*4),"read MFD cockpit composite");
            std::ofstream file(path,std::ios::binary);file<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<pixels.size();i+=4)file.write(reinterpret_cast<const char *>(pixels.data()+i),3);
            require(bool(file),"save MFD capture");
        }
        std::cout<<"Native original Orbit/Surface MFDs, selectors, focus ownership, addon callbacks and frame lifetime verified\n";
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
