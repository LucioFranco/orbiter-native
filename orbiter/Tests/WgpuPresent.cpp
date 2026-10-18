#include "Orbiter.h"
#include "Platform/SDLWindow.h"
#include "Platform/DynamicLibrary.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char **argv) {
    try {
        require(argc==3 || argc==4,"module and bitmap paths required; optional frame count");
        Orbiter host;
        std::filesystem::path path=argv[1];
        auto module=host.LoadModule(path.parent_path().string().c_str(),path.stem().string().c_str());
        require(module && host.GetGraphicsClient(),"load graphics module");
        auto gc=host.GetGraphicsClient();
        require(gc->clbkInitialise(),"initialize graphics");
        using Preview = bool (*)(SURFHANDLE);
        auto preview=reinterpret_cast<Preview>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_preview_surface"));
        require(preview != nullptr,"preview entry point");
        auto bitmap=oapiLoadSurfaceEx(argv[2],OAPISURFACE_TEXTURE,true);
        require(bitmap != nullptr,"load panel image");
        DWORD width,height;
        require(gc->clbkGetSurfaceSize(bitmap,&width,&height),"panel dimensions");
        auto composite=oapiCreateSurfaceEx(width,height,OAPISURFACE_RENDERTARGET);
        require(composite != nullptr,"create panel composite");
        oapiColourFill(composite,0x020610);
        require(gc->clbkSetSurfaceColourKey(bitmap,0xff00ff) &&
                gc->clbkBlt(composite,0,0,bitmap,BLT_SRCCOLORKEY),"composite panel with colour key");
        require(preview(composite),"select composited panel image");
        oapiDestroySurface(bitmap);
        oapiDestroySurface(composite); // presentation retains its own reference
        const unsigned frames=argc==4 ? std::stoul(argv[3]) : 8;
        {
            orbiter::platform::SDLWindow window(host);
            for (unsigned i=0;i<frames;++i) {
                require(window.Poll(),"window closed before test completed");
                if(i==2) require(SDL_SetWindowSize(window.Handle(),960,640),"resize window");
                host.RenderNativeFrame();
                SDL_Delay(16);
            }
            gc->clbkResizeNativeWindow(0,0);
            host.RenderNativeFrame(); // minimized swapchain is suspended
            int w,h; SDL_GetWindowSizeInPixels(window.Handle(),&w,&h);
            host.ResizeNativeViewport(w,h);
            host.RenderNativeFrame();
        }
        // Detach occurred before native window destruction; reattach is valid.
        {
            orbiter::platform::SDLWindow second(host);
            require(second.Poll(),"second window");
            host.RenderNativeFrame();
        }
        require(preview(nullptr),"release presentation reference");
        require(host.UnloadModule(module),"unload graphics module");
        std::cout << "Native presentation, resize, suspension and reattachment verified\n";
        return 0;
    } catch(const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
