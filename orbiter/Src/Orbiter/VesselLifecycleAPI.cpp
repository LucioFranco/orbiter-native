// Copyright (c) Martin Schweiger
// Licensed under the MIT License
#include "Orbiter.h"
#ifdef ORBITER_NATIVE_CORE
#include "NativeMFD.h"
#endif
#include "Psys.h"
#include "Vessel.h"
#include "Camera.h"
#include "Pane.h"
#ifndef ORBITER_NATIVE_CORE
#include "DlgMgr.h"
#endif
extern Orbiter *g_pOrbiter;
extern PlanetarySystem *g_psys;
extern Vessel *g_focusobj, *g_pfocusobj;
extern Camera *g_camera;
extern Pane *g_pane;

bool Orbiter::KillVessels ()
{
	int i, n = g_psys->nVessel();
	DWORD j;

	for (i = n-1; i >= 0; i--) {
		if (g_psys->GetVessel(i)->KillPending()) {
			Vessel *vessel = g_psys->GetVessel(i);
			// switch to new focus object
			if (vessel == g_focusobj) {
				if (vessel->ProxyVessel() && !vessel->ProxyVessel()->KillPending()) {
					SetFocusObject (vessel->ProxyVessel(), false);
				} else {
					double d, dmin = 1e20;
					Vessel *v, *tgt = 0;
					for (j = 0; j < g_psys->nVessel(); j++) {
						v = g_psys->GetVessel(j);
						if (v->KillPending()) continue;
						if (v != vessel && v->GetEnableFocus()) {
							d = vessel->GPos().dist (v->GPos());
							if (d < dmin) dmin = d, tgt = v;
						}
					}
					if (tgt) SetFocusObject (tgt, false);
					else return false; // no focus object available - give up
				}
			}
			if (vessel == g_pfocusobj)
				g_pfocusobj = 0; // clear previous focus (for Ctrl-F3 fast-switching)

			// switch to new camera target
			if (vessel == g_camera->Target())
				SetView (g_focusobj, 1);

			// broadcast vessel destruction to plugins
			for (auto it = m_Plugin.begin(); it != m_Plugin.end(); it++)
				it->pModule->clbkDeleteVessel((OBJHANDLE)vessel);

			if (gclient)
				gclient->clbkDeleteVessel((OBJHANDLE)vessel);
			// broadcast vessel destruction to all vessels
			g_psys->BroadcastVessel (MSG_KILLVESSEL, vessel);
			// broadcast vessel destruction to all MFDs
			if (g_pane) g_pane->DelVessel (vessel);
#ifdef ORBITER_NATIVE_CORE
            orbiter::NativeMFDForget(vessel);
#endif
			// broadcast vessel destruction to all open dialogs
			//if (gclient) gclient->clbkDialogBroadcast (MSG_KILLVESSEL, vessel);
#ifndef ORBITER_NATIVE_CORE
			if (pDlgMgr) pDlgMgr->BroadcastMessage (MSG_KILLVESSEL, vessel);
			// echo deletion on console window
			if (m_pConsole) {
				char cbuf[256];
				sprintf (cbuf, "Vessel %s deleted", vessel->Name());
				m_pConsole->Echo(cbuf);
			}
#endif
			// kill the vessel
			g_psys->DelVessel (vessel);
		}
	}
	return true;
}

DLLEXPORT OBJHANDLE oapiCreateVessel (const char *name, const char *classname, const VESSELSTATUS &status)
{
	Vessel *vessel = new Vessel (g_psys, name, classname, status); TRACENEW
	g_pOrbiter->InsertVessel (vessel);
	return (OBJHANDLE)vessel;
}

DLLEXPORT OBJHANDLE oapiCreateVesselEx (const char *name, const char *classname, const void *status)
{
	Vessel *vessel = new Vessel (g_psys, name, classname, status); TRACENEW
	g_pOrbiter->InsertVessel (vessel);
	return (OBJHANDLE)vessel;
}

DLLEXPORT bool oapiDeleteVessel (OBJHANDLE hVessel, OBJHANDLE hAlternativeCameraTarget)
{
	Vessel *vessel = (Vessel*)hVessel;
	if (g_camera->Target() == vessel && hAlternativeCameraTarget)
		g_pOrbiter->SetView ((Body*)hAlternativeCameraTarget, 1);
	vessel->RequestDestruct();
	return true;
}
