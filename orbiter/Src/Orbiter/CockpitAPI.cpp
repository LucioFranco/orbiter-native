// Copyright (c) Martin Schweiger
// Licensed under the MIT License
// Shared cockpit dispatch; absent cockpit means unavailable in headless hosts.
#include "Orbiter.h"
#ifdef ORBITER_NATIVE_CORE
#include "NativeMFD.h"
#endif
#include "Pane.h"
#include "GraphicsAPI.h"
#ifdef ORBITER_NATIVE_CORE
#include "Camera.h"
#include "NativeCockpit.h"
extern Camera *g_camera;
#endif
extern Orbiter *g_pOrbiter;
extern Pane *g_pane;

DLLEXPORT int oapiCockpitMode ()
{
	if (!g_pane) {
#ifdef ORBITER_NATIVE_CORE
        if (g_camera && g_camera->IsInternal() && g_camera->GetIntMode()==CAMERA_VIRTUALCOCKPIT) return COCKPIT_VIRTUAL;
#endif
        return 0;
    }
	return g_pane->GetPanelMode();
}

DLLEXPORT bool oapiSetHUDMode (int mode)
{
	return (g_pane ? g_pane->SetHUDMode (mode) : false);
}

DLLEXPORT bool oapiSetHUDMode (int mode, const HUDPARAM *prm)
{
	return (g_pane ? g_pane->SetHUDMode (mode, prm) : false);
}

DLLEXPORT int oapiGetHUDMode ()
{
	return (g_pane ? g_pane->GetHUDMode () : 0);
}

DLLEXPORT int oapiGetHUDMode (HUDPARAM *prm)
{
	return (g_pane ? g_pane->GetHUDMode (prm) : 0);
}

DLLEXPORT void oapiToggleMFD_on (int mfd)
{
#ifdef ORBITER_NATIVE_CORE
    orbiter::NativeMFDPower(mfd);return;
#endif

	if (g_pane) g_pane->ToggleMFD_on (mfd);
}

DLLEXPORT int oapiGetMFDMode (int mfd)
{
#ifdef ORBITER_NATIVE_CORE
    auto instrument=orbiter::NativeMFDGet(mfd);return instrument?instrument->Type():MFD_NONE;
#endif

	if (!g_pane) return MFD_NONE;
	return (g_pane->MFD(mfd) ? g_pane->MFD(mfd)->Type() : MFD_NONE);
}

DLLEXPORT int oapiSendMFDKey (int mfd, DWORD key)
{
#ifdef ORBITER_NATIVE_CORE
    auto instrument=orbiter::NativeMFDGet(mfd);if(!instrument)return 0;
    const bool accepted=instrument->ConsumeKeyBuffered(key);
    if(accepted)if(auto current=orbiter::NativeMFDGet(mfd))current->Refresh();
    return accepted;
#endif

	if (!g_pane) return 0;
	return g_pane->MFDConsumeKeyBuffered (mfd, key);
}

DLLEXPORT bool oapiProcessMFDButton (int mfd, int bt, int event)
{
#ifdef ORBITER_NATIVE_CORE
    auto instrument=orbiter::NativeMFDGet(mfd);if(!instrument || bt<0 || bt>=12)return false;
    const bool accepted=instrument->ConsumeButton(bt,event);
    if(accepted)if(auto current=orbiter::NativeMFDGet(mfd))current->Refresh();
    return accepted;
#endif

	if (!g_pane) return false;
	Instrument *MFD = g_pane->MFD(mfd);
	if (MFD) return MFD->ConsumeButton (bt, event);
	else     return false;
}

DLLEXPORT const char *oapiMFDButtonLabel (int mfd, int bt)
{
#ifdef ORBITER_NATIVE_CORE
    auto instrument=orbiter::NativeMFDGet(mfd);return instrument && bt>=0 && bt<12?instrument->ButtonLabel(bt):nullptr;
#endif

	if (!g_pane) return nullptr;
	return (g_pane->MFD(mfd) ? g_pane->MFD(mfd)->ButtonLabel(bt) : 0);
}

DLLEXPORT void oapiRegisterMFD (int mfd, const MFDSPEC &spec)
{
	if (!g_pane) return;
	g_pane->RegisterMFD (mfd, spec);
}

DLLEXPORT void oapiRegisterMFD (int mfd, const EXTMFDSPEC *spec)
{
	if (!g_pane) return;
	g_pane->RegisterMFD (mfd, spec);
}

DLLEXPORT void oapiRegisterPanelBackground (SURFHANDLE hBmp, DWORD flag, DWORD ck)
{
	if (g_pane) g_pane->RegisterPanelBackground (hBmp, flag, ck);
	else if (hBmp) {
		if (auto *gc = g_pOrbiter->GetGraphicsClient()) gc->clbkReleaseSurface(hBmp);
	}
}

DLLEXPORT void oapiRegisterPanelArea (int id, const RECT &pos, int draw_event, int mouse_event, int bkmode)
{
	if (!g_pane) return;
	g_pane->RegisterPanelArea (id, pos, draw_event, mouse_event, bkmode);
}

DLLEXPORT void oapiSetPanelNeighbours (int left, int right, int top, int bottom)
{
	if (!g_pane) return;
	g_pane->SetPanelNeighbours (left, right, top, bottom);
}

#ifdef ORBITER_NATIVE_CORE
DLLEXPORT void oapiVCSetCurrentView(int id)
{
    if(id>=0)orbiter::NativeCockpitState().cockpitId=id;
}
#endif

DLLEXPORT void oapiVCSetNeighbours (int left, int right, int top, int bottom)
{
    if (!g_pane) {
#ifdef ORBITER_NATIVE_CORE
        auto &state=orbiter::NativeCockpitState();
        state.neighbours[0]=left;state.neighbours[1]=right;state.neighbours[2]=top;state.neighbours[3]=bottom;
#endif
        return;
    }
	g_pane->SetVCNeighbours (left, right, top, bottom);
}

DLLEXPORT int oapiSetPanel (int panel_id)
{
	if (!g_pane) return -1;
	return g_pane->SelectPanel (panel_id);
}

DLLEXPORT void oapiVCRegisterArea (int id, const RECT &tgtrect, int draw_event, int mouse_event, int bkmode, SURFHANDLE tgt)
{
	if (!g_pane) {
#ifdef ORBITER_NATIVE_CORE
        orbiter::NativeCockpitState().RegisterDraw(id,tgtrect,draw_event,mouse_event,bkmode,tgt);
#endif
        return;
    }
	g_pane->RegisterVCArea (id, tgtrect, draw_event, mouse_event, bkmode, tgt);
}

DLLEXPORT void oapiVCRegisterArea (int id, int draw_event, int mouse_event)
{
	if (!g_pane) {
#ifdef ORBITER_NATIVE_CORE
        orbiter::NativeCockpitState().RegisterDraw(id,_R(0,0,0,0),draw_event,mouse_event,PANEL_MAP_NONE,nullptr);
#endif
        return;
    }
	g_pane->RegisterVCArea (id, _R(0,0,0,0), draw_event, mouse_event, PANEL_MAP_NONE, NULL);
}

DLLEXPORT void oapiVCSetAreaClickmode_Spherical (int id, const VECTOR3 &cnt, double rad)
{
	if (!g_pane) {
#ifdef ORBITER_NATIVE_CORE
        orbiter::NativeCockpitState().Sphere(id,cnt,rad);
#endif
        return;
    }
	g_pane->SetVCAreaClickmode_Spherical (id, Vector(cnt.x, cnt.y, cnt.z), rad);
}

DLLEXPORT void oapiVCSetAreaClickmode_Quadrilateral (int id, const VECTOR3 &p1, const VECTOR3 &p2, const VECTOR3 &p3, const VECTOR3 &p4)
{
	if (!g_pane) {
#ifdef ORBITER_NATIVE_CORE
        orbiter::NativeCockpitState().Quad(id,p1,p2,p3,p4);
#endif
        return;
    }
	g_pane->SetVCAreaClickmode_Quadrilateral (id, Vector(p1.x, p1.y, p1.z), Vector(p2.x,p2.y,p2.z), Vector(p3.x,p3.y,p3.z), Vector(p4.x,p4.y,p4.z));
}

#ifdef ORBITER_NATIVE_CORE
DLLEXPORT void oapiVCTriggerRedrawArea(int vc_id,int area_id) {
    orbiter::NativeCockpitState().Trigger(vc_id,area_id);
}
DLLEXPORT void oapiTriggerRedrawArea(int,int vc_id,int area_id) {
    orbiter::NativeCockpitState().Trigger(vc_id,area_id);
}
DLLEXPORT bool oapiBltPanelAreaBackground(int area_id,SURFHANDLE target) {
    return orbiter::NativeCockpitState().BltBackground(area_id,target);
}
#endif

DLLEXPORT void oapiVCRegisterMFD(int mfd,const VCMFDSPEC *spec) {
#ifdef ORBITER_NATIVE_CORE
    if (spec && mfd>=0 && mfd<MAXMFD) orbiter::NativeCockpitState().mfdScreens[mfd]=*spec;
#else
    if (g_pane) g_pane->RegisterVCMFD(mfd,spec);
#endif
}
