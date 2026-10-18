#include "Orbiter.h"
#include "Camera.h"
#include "NativeCockpit.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
extern Camera *g_camera;
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==3,"graphics module and CSM runtime required");
        auto plugin=std::filesystem::absolute(argv[1]);std::filesystem::current_path(argv[2]);
        Orbiter host;auto module=host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str());
        require(module && host.GetGraphicsClient()->clbkInitialise(),"initialize UI graphics");
        host.Launch("NasspCSM");require(host.LoadNativeVirtualCockpit(),"load CSM cockpit");
        orbiter::platform::SDLWindow window(host,false,true);SDL_RaiseWindow(window.Handle());
        auto frame=[&]() {require(window.Poll(),"poll UI frame");host.RenderNativeFrame();};
        frame();frame();
        auto key=[&](SDL_Scancode scancode,bool down) {
            SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
            e.key.windowID=SDL_GetWindowID(window.Handle());e.key.scancode=scancode;
            e.key.key=SDL_GetKeyFromScancode(scancode,SDL_KMOD_NONE,false);e.key.down=down;
            require(SDL_PushEvent(&e) && window.Poll(),"dispatch UI key");
        };
        key(SDL_SCANCODE_K,true);require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"flight key held before UI opens");
        key(SDL_SCANCODE_F1,true);key(SDL_SCANCODE_F1,false);
        require(host.NativeUIVisible() && !KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"opening panel releases held flight control");
        frame();frame();
        auto panel=ImGui::FindWindowByName("Flight controls");require(panel && panel->Active,"flight panel exists in shared context");
        // First button follows two text rows inside the title bar and padding.
        auto &style=ImGui::GetStyle();
        const float x=panel->InnerRect.Min.x+style.WindowPadding.x+30;
        const float y=panel->InnerRect.Min.y+style.WindowPadding.y+2*(panel->FontRefSize+style.ItemSpacing.y)+(panel->FontRefSize+2*style.FramePadding.y)/2;
        SDL_WarpMouseInWindow(window.Handle(),1000,600);frame();frame();
        require(!ImGui::GetIO().WantCaptureMouse,"mouse begins outside panel");
        const double aperture=g_camera->Aperture();
        SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.windowID=SDL_GetWindowID(window.Handle());
        wheel.wheel.mouse_x=x;wheel.wheel.mouse_y=y;wheel.wheel.y=3;
        require(SDL_PushEvent(&wheel),"queue immediate wheel over panel");frame();
        require(g_camera->Aperture()==aperture,"first event over panel cannot zoom cockpit through stale capture flags");
        SDL_WarpMouseInWindow(window.Handle(),x,y);frame();frame();
        require(ImGui::GetIO().WantCaptureMouse,"hovering flight panel captures mouse");
        auto click=[&](bool down) {
            SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
            e.button.windowID=SDL_GetWindowID(window.Handle());e.button.button=SDL_BUTTON_LEFT;e.button.down=down;e.button.x=x;e.button.y=y;
            require(SDL_PushEvent(&e),"queue UI click");frame();
        };
        click(true);require(!orbiter::NativeCockpitState().capture[0],"UI click does not capture a cockpit control");click(false);
        host.StepSimulation(1.0/60);frame();require(!host.IsRunning(),"SDL button pauses original simulation");
        require(ImGui::GetIO().WantCaptureKeyboard,"focused UI captures keyboard");
        key(SDL_SCANCODE_F5,true);key(SDL_SCANCODE_F5,false);host.StepSimulation(1.0/60);frame();
        require(host.IsRunning(),"F5 remains a global shortcut while UI is focused");
        key(SDL_SCANCODE_F5,true);key(SDL_SCANCODE_F5,false);host.StepSimulation(1.0/60);frame();
        require(!host.IsRunning(),"global F5 pauses again");
        key(SDL_SCANCODE_K,false);key(SDL_SCANCODE_K,true);
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"captured UI keyboard cannot drive spacecraft");key(SDL_SCANCODE_K,false);
        using Read=bool (*)(BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));require(read,"read UI capture");
        std::vector<BYTE> overlay(size_t(host.ViewW())*host.ViewH()*4),cockpit(overlay.size());
        require(read(overlay.data(),overlay.size(),host.ViewW()*4),"read visible flight panel");
        if(const char *path=std::getenv("ORBITER_UI_CAPTURE")) {
            std::ofstream image(path,std::ios::binary);image<<"P6\n"<<host.ViewW()<<' '<<host.ViewH()<<"\n255\n";
            for(size_t i=0;i<overlay.size();i+=4)image.write(reinterpret_cast<const char *>(overlay.data()+i),3);
            require(bool(image),"write UI frame");
        }
        const float viewButtonY=y+panel->FontRefSize+2*style.FramePadding.y+style.ItemSpacing.y;
        // The preceding rapid key gestures are deliberately trickled across
        // ImGui frames. Finish them before starting a separate pointer gesture.
        auto settleInput=[&]() {
            for(unsigned i=0;i<32 && GImGui->InputEventsQueue.Size;++i)frame();
            require(GImGui->InputEventsQueue.Size==0,"drain trickled ImGui input before next gesture");
        };
        auto viewButton=[&]() {
            settleInput();
            SDL_WarpMouseInWindow(window.Handle(),x,viewButtonY);
            SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID=SDL_GetWindowID(window.Handle());motion.motion.x=x;motion.motion.y=viewButtonY;
            require(SDL_PushEvent(&motion),"queue view-button pointer motion");frame();settleInput();
            for(bool down:{true,false}) {
                SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.windowID=SDL_GetWindowID(window.Handle());event.button.button=SDL_BUTTON_LEFT;
                event.button.down=down;event.button.x=x;event.button.y=viewButtonY;
                require(SDL_PushEvent(&event),"queue view button click");frame();settleInput();
            }
            frame();
        };
        require(g_camera->IsInternal(),"view control starts in cockpit");
        viewButton();require(!g_camera->IsInternal(),"SDL exterior button switches the actual camera");
        std::vector<BYTE> exterior(overlay.size());
        require(read(exterior.data(),exterior.size(),host.ViewW()*4),"read exterior after UI switch");
        size_t exteriorChanged=0;for(size_t i=0;i<exterior.size();i+=4)
            exteriorChanged+=exterior[i]!=overlay[i] || exterior[i+1]!=overlay[i+1] || exterior[i+2]!=overlay[i+2];
        require(exteriorChanged>10000,"view button changes rendered spacecraft scene");
        SDL_WarpMouseInWindow(window.Handle(),1000,600);frame();frame();
        const double exteriorDistance=g_camera->Distance();
        wheel.wheel.mouse_x=1000;wheel.wheel.mouse_y=600;wheel.wheel.y=2;
        require(SDL_PushEvent(&wheel),"queue exterior zoom");frame();
        require(g_camera->Distance()!=exteriorDistance,"SDL wheel zooms exterior camera outside panel");
        viewButton();require(g_camera->IsInternal(),"SDL cockpit button restores the 3D cockpit");
        require(!orbiter::NativeCockpitState().areas.empty(),"restored cockpit retains interactive controls");
        SDL_WarpMouseInWindow(window.Handle(),1000,600);frame();frame();
        for(bool down:{true,false}) {
            SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
            e.button.windowID=SDL_GetWindowID(window.Handle());e.button.button=SDL_BUTTON_LEFT;
            e.button.down=down;e.button.x=1000;e.button.y=600;
            require(SDL_PushEvent(&e),"queue click outside panel");frame();
        }
        frame();frame();key(SDL_SCANCODE_K,true);
        require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"clicking outside returns flight keyboard focus while panel stays open");key(SDL_SCANCODE_K,false);
        key(SDL_SCANCODE_F1,true);key(SDL_SCANCODE_F1,false);frame();frame();
        require(!host.NativeUIVisible(),"F1 closes panel despite UI keyboard capture");
        require(read(cockpit.data(),cockpit.size(),host.ViewW()*4),"read unobscured cockpit");
        size_t changed=0;for(size_t i=0;i<overlay.size();i+=4)changed+=overlay[i]!=cockpit[i] || overlay[i+1]!=cockpit[i+1] || overlay[i+2]!=cockpit[i+2];
        require(changed>10000,"ImGui is composited over actual 3D cockpit");
        key(SDL_SCANCODE_K,true);require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"closing panel restores flight keyboard routing");key(SDL_SCANCODE_K,false);
        frame();frame();
        require(read(cockpit.data(),cockpit.size(),host.ViewW()*4),"read annotation-free cockpit");
        auto note=oapiCreateAnnotation(false,1.0,_V(1,1,0));
        require(note!=nullptr,"create original SDK annotation");
        oapiAnnotationSetPos(note,0.65,0.7,0.95,0.95);
        oapiAnnotationSetText(note,const_cast<char *>("MCC INSERTION CHECK"));frame();
        std::vector<BYTE> annotated(cockpit.size());
        require(read(annotated.data(),annotated.size(),host.ViewW()*4),"read original annotation over cockpit");
        size_t annotationPixels=0;
        for(size_t i=0;i<annotated.size();i+=4)
            annotationPixels+=annotated[i]!=cockpit[i] || annotated[i+1]!=cockpit[i+1] || annotated[i+2]!=cockpit[i+2];
        require(annotationPixels>100,"original SDK text is rendered over the spacecraft");
        oapiAnnotationSetText(note,nullptr);frame();
        require(read(annotated.data(),annotated.size(),host.ViewW()*4) && annotated==cockpit,"clearing annotation restores the scene");
        require(oapiDelAnnotation(note),"delete the final annotation and release resources");
        std::cout<<"F1 flight overlay, real SDL pause/view buttons, exterior zoom, cockpit isolation and input release verified\n";
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
