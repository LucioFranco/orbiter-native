#include "Orbiter.h"
#include "Vessel.h"
#include "NativeMFD.h"
#include "Mfd.h"
#include "Select.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
extern InputBox *g_input;
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
static std::string saved(const char *path) {std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
static std::string flashState(const std::string &state) {
    const auto begin=state.find("Columbia:");const auto field=state.find("FLASHING ",begin);
    require(begin!=std::string::npos && field!=std::string::npos,"CSM checklist state saved");
    return state.substr(field,10);
}
static int buttonIndex(int mfd,const char *wanted) {
    for(int button=0;button<12;++button) {
        const char *label=oapiMFDButtonLabel(mfd,button);
        if(label && std::string(label)==wanted) return button;
    }
    throw std::runtime_error(std::string("Original MFD control missing: ")+wanted);
}
int main(int argc,char **argv) {
    try {
        const bool resume=argc==5 && std::string(argv[4])=="--resume";
        require(argc==4 || resume,"graphics plugin, NASSP MFD plugin and pair runtime required");
        auto graphics=std::filesystem::absolute(argv[1]),addon=std::filesystem::absolute(argv[2]);
        std::filesystem::current_path(argv[3]);Orbiter host;
        auto module=host.LoadModule(graphics.parent_path().string().c_str(),graphics.stem().string().c_str());auto gc=host.GetGraphicsClient();
        require(module && gc && gc->clbkInitialise(),"initialize graphics");
        require(host.LoadModule(addon.parent_path().string().c_str(),addon.stem().string().c_str()),"load original NASSP MFD plugin");
        host.Launch(resume?"NasspMFDSaved":"NasspPair");
        require(host.LoadNativeVirtualCockpit(),"load CSM cockpit");orbiter::platform::SDLWindow window(host,false,true);
        for(int i=0;i<60;++i)host.StepSimulation(1.0/60);
        char mainName[]="Project Apollo",checkName[]="Project Apollo Checklist";MFDMODESPECEX *spec=nullptr;
        const int mainMode=oapiGetMFDModeSpecEx(mainName,&spec),checkMode=oapiGetMFDModeSpecEx(checkName,&spec);
        require(mainMode>BUILTIN_MFD_MODES && checkMode>BUILTIN_MFD_MODES,"both original addon modes registered");
        if(!resume) {oapiOpenMFD(mainMode,0);oapiOpenMFD(checkMode,1);}
        require(oapiGetMFDMode(0)==mainMode && oapiGetMFDMode(1)==checkMode,"NASSP modes opened or restored");
        using Read=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));require(read,"read MFD pixels");
        auto image=[&](int id) {auto instrument=orbiter::NativeMFDGet(id);require(instrument,"instrument exists");instrument->Update(1);std::vector<BYTE> out(512*512*4);require(read(instrument->Surface(),out.data(),out.size(),2048),"read addon surface");return out;};
        auto mainImage=image(0),checkImage=image(1);size_t green=0;
        for(size_t i=0;i<checkImage.size();i+=4)green+=checkImage[i+1]>100 && checkImage[i]<80;
        require(green>1000 && mainImage!=checkImage,"NASSP checklist text and main page rendered");
        require(std::string(oapiMFDButtonLabel(0,0))=="GNC" && std::string(oapiMFDButtonLabel(1,0))=="NAV","original NASSP button labels");
        if(resume) {std::cout<<"Original NASSP MFD modes restored and rendered in fresh process\n";return 0;}
        require(host.SaveScenario("NasspMFDBefore","Checklist state",0),"save pre-button state");
        require(oapiProcessMFDButton(1,buttonIndex(1,"FLSH"),PANEL_MOUSE_LBDOWN),"toggle checklist flashing through original MFD");
        require(host.SaveScenario("NasspMFDAfter","Checklist state",0),"save post-button state");
        require(flashState(saved("Scenarios/NasspMFDBefore.scn"))!=flashState(saved("Scenarios/NasspMFDAfter.scn")),"MFD connector changes the real CSM checklist controller");
        host.Pause(true);host.StepSimulation(1.0/60);
        require(oapiProcessMFDButton(1,0,PANEL_MOUSE_LBDOWN),"open checklist navigation while paused");
        require(image(1)!=checkImage && std::string(oapiMFDButtonLabel(1,0))=="BCK","original checklist navigation draws and changes controls");
        require(oapiProcessMFDButton(1,0,PANEL_MOUSE_LBDOWN),"return to active checklist");
        require(oapiProcessMFDButton(0,0,PANEL_MOUSE_LBDOWN),"open GNC page");
        require(image(0)!=mainImage && std::string(oapiMFDButtonLabel(0,0))=="BCK","original GNC page rendered");
        require(oapiProcessMFDButton(0,buttonIndex(0,"TLO"),PANEL_MOUSE_LBDOWN) && g_input->IsActive(),"NASSP input dialog uses native host");
        char invalid[]="not a time";require(!g_input->cbEnter(g_input,invalid,g_input->userdata),"original input validation rejects bad launch time");
        orbiter::NativeDialogsReset();oapiProcessMFDButton(0,0,PANEL_MOUSE_LBDOWN);
        host.Pause(false);host.StepSimulation(1.0/60);
        const auto csm=oapiGetFocusObject();require(host.CycleNativeFocus() && oapiGetFocusObject()!=csm,"focus Eagle");
        oapiOpenMFD(mainMode,0);oapiOpenMFD(checkMode,1);
        require(std::string(oapiMFDButtonLabel(0,0))=="GNC" && image(1)!=checkImage,"NASSP MFDs support Eagle with its own checklist");
        require(host.CycleNativeFocus() && oapiGetFocusObject()==csm,"return to Columbia");
        orbiter::NativeMFDToggle();host.RenderNativeFrame();host.RenderNativeFrame();
        require(host.SaveScenario("NasspMFDSaved","Native NASSP MFDs",0),"save addon MFD scenario");
        if(const char *path=std::getenv("ORBITER_NASSP_MFD_CAPTURE")) {
            using Output=bool (*)(BYTE *,size_t,DWORD);auto output=reinterpret_cast<Output>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));
            std::vector<BYTE> pixels(size_t(host.ViewW())*host.ViewH()*4);require(output && output(pixels.data(),pixels.size(),host.ViewW()*4),"read cockpit composite");
            std::ofstream out(path,std::ios::binary);out<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<pixels.size();i+=4)out.write(reinterpret_cast<const char *>(pixels.data()+i),3);
        }
        std::cout<<"Original NASSP MFDs, checklist controller, navigation, input dialogs and both spacecraft verified\n";
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
