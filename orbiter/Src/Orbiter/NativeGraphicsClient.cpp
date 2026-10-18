// Native host support for the existing graphics-client ABI.
#include "Orbiter.h"
#include "GraphicsAPI.h"
#include "Body.h"
#include "Camera.h"
#include "NativeCockpit.h"
#include "NativeMFD.h"
#include "Mfd.h"

extern Orbiter *g_pOrbiter;
extern Camera *g_camera;
namespace oapi {
GraphicsClient::GraphicsClient(HINSTANCE instance) : Module(instance), hVid(nullptr),
    surfBltTgt(RENDERTGT_NONE), splashFont(nullptr), hRenderWnd(nullptr),
    hOrbiterInst(g_pOrbiter ? g_pOrbiter->GetInstance() : nullptr), VideoData{}, m_pIWICFactory(nullptr) {}
GraphicsClient::~GraphicsClient() = default;
bool GraphicsClient::clbkInitialise() { return true; }
// The SDL host owns the window; Win32 window procedures and GDI are unavailable.
HWND GraphicsClient::clbkCreateRenderWindow() { return nullptr; }
void GraphicsClient::clbkDestroyRenderWindow(bool) {}
LRESULT GraphicsClient::RenderWndProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
INT_PTR GraphicsClient::LaunchpadVideoWndProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
SURFHANDLE GraphicsClient::clbkCreateSurface(HBITMAP) { return nullptr; }
bool GraphicsClient::clbkCopyBitmap(SURFHANDLE, HBITMAP, int, int, int, int) { return false; }
ScreenAnnotation *GraphicsClient::clbkCreateAnnotation() { return new ScreenAnnotation(this); }
}
bool Orbiter::AttachGraphicsClient(oapi::GraphicsClient *gc) {
    if (!gc || gclient) return false;
    register_module = gc;
    gclient = gc;
    return true;
}
bool Orbiter::RemoveGraphicsClient(oapi::GraphicsClient *gc) {
    if (!gclient || gclient != gc) return false;
    gclient = nullptr;
    return true;
}
DLLEXPORT bool oapiRegisterGraphicsClient(oapi::GraphicsClient *gc) { return g_pOrbiter->AttachGraphicsClient(gc); }
DLLEXPORT bool oapiUnregisterGraphicsClient(oapi::GraphicsClient *gc) { return g_pOrbiter->RemoveGraphicsClient(gc); }

namespace oapi {
void GraphicsClient::RegisterVisObject(OBJHANDLE object,VISHANDLE visual) {if(object)static_cast<Body *>(object)->RegisterVisual(visual);}
void GraphicsClient::UnregisterVisObject(OBJHANDLE object) {if(object)static_cast<Body *>(object)->UnregisterVisual();}
}

namespace oapi {
SURFHANDLE GraphicsClient::GetMFDSurface(int id) const {
    auto instrument=orbiter::NativeMFDGet(id);
    return instrument?instrument->Surface():nullptr;
}
SURFHANDLE GraphicsClient::GetVCMFDSurface(int id,const VCMFDSPEC **spec) const {
    if(!spec)return nullptr;
    *spec=nullptr;
    const auto &cockpit=orbiter::NativeCockpitState();
    if(!g_camera || !g_camera->IsInternal() || oapiCockpitMode()!=COCKPIT_VIRTUAL ||
       cockpit.owner!=oapiGetFocusObject())return nullptr;
    auto found=cockpit.mfdScreens.find(id);
    if(found==cockpit.mfdScreens.end())return nullptr;
    *spec=&found->second;
    return GetMFDSurface(id);
}
}
