#include "Orbiter.h"
#include "Camera.h"
#include "Vessel.h"
#include "NativeCockpit.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
extern Camera *g_camera;
static void require(bool value,const char *why) {if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==5 || argc==6,"graphics module, runtime, scenario and saved view required");
        const auto plugin=std::filesystem::absolute(argv[1]);
        std::filesystem::current_path(argv[2]);Orbiter host;
        require(host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str()) &&
            host.GetGraphicsClient() && host.GetGraphicsClient()->clbkInitialise(),"load graphics client");
        host.Launch(argv[3]);host.ResizeNativeViewport(1280,720);
        auto vessel=reinterpret_cast<Vessel *>(oapiGetFocusObject());
        auto savedView=[&]() {
            std::ostringstream output;vessel->Write(output);std::istringstream lines(output.str());std::string line;
            while(std::getline(lines,line)) {std::istringstream fields(line);std::string key;int view;
                if(fields>>key>>view && key=="VIEWPOS")return view;}
            throw std::runtime_error("NASSP did not write VIEWPOS");
        };
        require(host.LoadNativeVirtualCockpit(),"restore saved cockpit");
        const int expected=std::stoi(argv[4]);
        require(orbiter::NativeCockpitState().cockpitId==expected,"registered cockpit ID matches saved seat");
        require(savedView()==expected,"vessel restored saved seat");
        const unsigned directions[]={OAPI_KEY_LEFT,OAPI_KEY_RIGHT,OAPI_KEY_UP,OAPI_KEY_DOWN};
        int neighbour=-1;
        for(unsigned direction=0;direction<4;++direction) {
            neighbour=orbiter::NativeCockpitState().neighbours[direction];if(neighbour<0)continue;
            host.NativeKeyEvent(OAPI_KEY_LCONTROL,true);host.NativeKeyEvent(directions[direction],true);
            host.ReleaseNativeKeys();break;
        }
        require(neighbour>=0 && savedView()==neighbour && orbiter::NativeCockpitState().cockpitId==neighbour,"navigation from restored seat uses its actual neighbours");
        // No simulation tick yet: explicit requests must still select their ID.
        for(int id=0;id<12;++id) {
            require(host.LoadNativeVirtualCockpit(id),"load explicit NASSP cockpit seat");
            require(savedView()==id && orbiter::NativeCockpitState().cockpitId==id,"vessel and host agree before first tick");
            const auto eye=g_camera->CockpitPos();VECTOR3 direction{};oapiCameraGlobalDir(&direction);
            require(!orbiter::NativeCockpitState().areas.empty(),"seat has interactive areas");
            auto api=oapiGetVesselInterface(oapiGetFocusObject());
            api->SetCameraDefaultDirection(_V(1,0,0));
            g_camera->MouseButton(1,true);g_camera->MouseMotion(400,-250);g_camera->MouseButton(1,false);
            require(host.LoadNativeVirtualCockpit(id),"restore disturbed camera seat");
            VECTOR3 restored{};oapiCameraGlobalDir(&restored);
            require(length(restored-direction)<1e-9 && (g_camera->CockpitPos()-eye).length()<1e-9,"seat restores direction and eye position");
        }
        host.StepSimulation(1.0/60.0);
        require(savedView()==11 && orbiter::NativeCockpitState().cockpitId==11,"first timestep preserves explicit viewpoint selection");
        if(argc==6)require(host.SaveScenario(argv[5],"Native viewpoint resume regression",0),"save selected cockpit viewpoint");
        std::cout<<"Saved view and all 12 explicit NASSP cockpit viewpoints verified\n";
    }catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
