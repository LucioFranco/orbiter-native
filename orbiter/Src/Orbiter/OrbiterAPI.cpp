// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define STRICT 1
#define OAPI_IMPLEMENTATION

#include "Orbiter.h"
#include "Launchpad.h"
#include "Psys.h"
#include "Camera.h"
#include "Pane.h"
#include "Defpanel.h"
#include "Panel2D.h"
#include "Vessel.h"
#include "Select.h"
#include "DlgMgr.h"
#include "Config.h"
#include "cmdline.h"
#include "Script.h"
#include "Util.h"
#include "Log.h"
#include "Dialogs.h"
#include "resource.h"
#include "Mesh.h"
#include "MenuInfoBar.h"
#include <zlib.h>
#include "DrawAPI.h"

#include "Orbitersdk.h"

using namespace std;

extern Orbiter *g_pOrbiter;
extern TimeData td;
extern PlanetarySystem *g_psys;
extern Camera *g_camera;
extern Pane *g_pane;
extern Vessel *g_focusobj;
extern InputBox *g_input;
extern bool g_bShowGrapple;
extern char DBG_MSG[256];

// ------------------------------------------------------------------------------
// API interface functions
// ------------------------------------------------------------------------------

DLLEXPORT void FormatValue (char *cbuf, int n, double f, int precision)
{
	char *s = FloatStr (f, precision);
	strncpy (cbuf, s, n);
}

DLLEXPORT HINSTANCE oapiGetOrbiterInstance ()
{
	return g_pOrbiter->GetInstance();
}

DLLEXPORT int oapiGetOrbiterVersion ()
{
	return g_pOrbiter->GetVersion();
}

DLLEXPORT const char *oapiGetCmdLine ()
{
	return orbiter::CommandLine::Instance().CmdLine();
}



DLLEXPORT double oapiGetPanel2DScale ()
{
	Panel2D *p = g_pane->GetPanel2D();
	if (p) return p->GetActiveScale();
	else   return 1.0;
}

DLLEXPORT void oapiRegisterModule (oapi::Module *module)
{
	g_pOrbiter->register_module = module;
}


DLLEXPORT OBJHANDLE oapiGetObjectByIndex (int index)
{
	return (OBJHANDLE)(g_psys ? g_psys->GetObj (index) : 0);
}

DLLEXPORT DWORD oapiGetObjectCount ()
{
	return (g_psys ? g_psys->nObj() : 0);
}

DLLEXPORT int oapiGetObjectType (OBJHANDLE hObj)
{
	int i;

	Body *body = (Body*)hObj;

	// Try for celestial body
	CelestialBody *cbody = 0;
	try {
		cbody = dynamic_cast<CelestialBody*>(body);
	}
	catch(...) {
		cbody = 0;
	}
	if (cbody) return body->Type();

	// Try for surface base
	Base *base = 0;
	try {
		base = dynamic_cast<Base*>(body);
	}
	catch(...) {
		base = 0;
	}
	if (base) return body->Type();

	// Try for vessel. Here we go through the list of current
	// vessels, to avoid deleted vessels (which may still cast ok)
	for (i = 0; i < g_psys->nVessel(); i++) {
		if (body == g_psys->GetVessel(i))
			return body->Type();
	}

	return OBJTP_INVALID;
}

DLLEXPORT const void *oapiGetObjectParam (OBJHANDLE hObj, DWORD paramtype)
{
	switch (((Body*)hObj)->Type()) {
	case OBJTP_VESSEL:
		return ((Vessel*)hObj)->GetParam (paramtype);
	case OBJTP_PLANET:
		return ((Planet*)hObj)->GetParam (paramtype);
	default:
		return ((Body*)hObj)->GetParam (paramtype);
	}
}





DLLEXPORT OBJHANDLE oapiGetStationByName (char *name)
{
	static bool bWarning = true;
	if (bWarning) {
		LogOut_Obsolete(__FUNCTION__);
		bWarning = false;
	}
	return 0;
}

DLLEXPORT OBJHANDLE oapiGetStationByIndex (int index)
{
	static bool bWarning = true;
	if (bWarning) {
		LogOut_Obsolete(__FUNCTION__);
		bWarning = false;
	}
	return 0;
}

DLLEXPORT DWORD oapiGetStationCount ()
{
	static bool bWarning = true;
	if (bWarning) {
		LogOut_Obsolete(__FUNCTION__);
		bWarning = false;
	}
	return 0;
}






DLLEXPORT OBJHANDLE oapiGetBaseByName (OBJHANDLE hPlanet, char *name)
{
	Body *body = (Body*)hPlanet;
	return (OBJHANDLE)(body->Type() == OBJTP_PLANET ? ((Planet*)hPlanet)->GetBase (name, true) : 0);
}

DLLEXPORT OBJHANDLE oapiGetBaseByIndex (OBJHANDLE hPlanet, int index)
{
	Planet *p = (Planet*)hPlanet;
	if (index < 0 || index >= p->nBase()) return 0;
	return (OBJHANDLE)p->GetBase (index);
}

DLLEXPORT DWORD oapiGetBaseCount (OBJHANDLE hPlanet)
{
	Body *body = (Body*)hPlanet;
	return (body->Type() == OBJTP_PLANET ? ((Planet*)body)->nBase() : 0);
}

DLLEXPORT OBJHANDLE oapiGetBasePlanet (OBJHANDLE hBase)
{
	return (OBJHANDLE)((Base*)hBase)->RefPlanet();
}

DLLEXPORT void oapiGetObjectName (OBJHANDLE hObj, char *name, int n)
{
	strncpy (name, ((Body*)hObj)->Name(), n);
}

DLLEXPORT const char *oapiGetObjectFileName(OBJHANDLE hObj)
{
	return ((Body*)hObj)->FileName();
}











DLLEXPORT double oapiGetEmptyMass (OBJHANDLE hVessel)
{
	return ((Vessel*)hVessel)->EmptyMass();
}

DLLEXPORT double oapiGetFuelMass (OBJHANDLE hVessel)
{
	return ((Vessel*)hVessel)->FuelMass();
}

DLLEXPORT double oapiGetMaxFuelMass (OBJHANDLE hVessel)
{
	return (((Vessel*)hVessel)->ntank ? ((Vessel*)hVessel)->tank[0]->maxmass : 0.0);
}

DLLEXPORT double oapiGetPropellantMaxMass (PROPELLANT_HANDLE ph)
{
	return ((TankSpec*)ph)->maxmass;
}

DLLEXPORT double oapiGetPropellantMass (PROPELLANT_HANDLE ph)
{
	return ((TankSpec*)ph)->mass;
}

DLLEXPORT PROPELLANT_HANDLE oapiGetPropellantHandle (OBJHANDLE hVessel, DWORD idx)
{
	Vessel *vessel = (Vessel*)hVessel;
	return (idx < vessel->ntank ? (PROPELLANT_HANDLE)vessel->tank[idx] : 0);
}

DLLEXPORT DOCKHANDLE oapiGetDockHandle (OBJHANDLE hVessel, UINT n)
{
	Vessel *vessel = (Vessel*)hVessel;
	if (n < vessel->nDock()) return (DOCKHANDLE)vessel->GetDockParams(n);
	else return 0;
}

DLLEXPORT OBJHANDLE oapiGetDockStatus (DOCKHANDLE dock)
{
	return (OBJHANDLE)((PortSpec*)dock)->mate;
}

DLLEXPORT void oapiSetAutoCapture(DOCKHANDLE hDock, bool enable)
{
	((PortSpec*)hDock)->autodock = enable;
}

DLLEXPORT OBJHANDLE oapiGetDockOwner(DOCKHANDLE hDock)
{
	return (OBJHANDLE)((PortSpec*)hDock)->owner;
}

DLLEXPORT void oapiSetEmptyMass (OBJHANDLE hVessel, double mass)
{
	if ((Vessel*)hVessel) ((Vessel*)hVessel)->SetEmptyMass (mass);
}



DLLEXPORT void oapiGetFocusGlobalPos (VECTOR3 *pos)
{
	Vector gp(g_focusobj->GPos());
	pos->x = gp.x, pos->y = gp.y, pos->z = gp.z;
}

DLLEXPORT void oapiGetFocusGlobalVel (VECTOR3 *vel)
{
	Vector gv(g_focusobj->GVel());
	vel->x = gv.x, vel->y = gv.y, vel->z = gv.z;
}



DLLEXPORT void oapiGetFocusRelativePos (OBJHANDLE hRef, VECTOR3 *pos)
{
	if (((Body*)hRef)->s0) {
		Vector dp(g_focusobj->GPos() - ((Body*)hRef)->GPos());
		pos->x = dp.x, pos->y = dp.y, pos->z = dp.z;
	}
}

DLLEXPORT void oapiGetFocusRelativeVel (OBJHANDLE hRef, VECTOR3 *vel)
{
	if (((Body*)hRef)->s0) {
		Vector dv(g_focusobj->GVel() - ((Body*)hRef)->GVel());
		vel->x = dv.x, vel->y = dv.y, vel->z = dv.z;
	}
}

DLLEXPORT void oapiGetBarycentre (OBJHANDLE hObj, VECTOR3 *bary)
{
	if (((Body*)hObj)->s0) {
		Vector b(((CelestialBody*)hObj)->Barycentre());
		bary->x = b.x, bary->y = b.y, bary->z = b.z;
	}
}


DLLEXPORT void oapiGlobalToLocal (OBJHANDLE hObj, const VECTOR3 *glob, VECTOR3 *loc)
{
	if (((Body*)hObj)->s0) {
		Vector vloc;
		((Body*)hObj)->GlobalToLocal(Vector(glob->x, glob->y, glob->z), vloc);
		loc->x = vloc.x, loc->y = vloc.y, loc->z = vloc.z;
	}
}

DLLEXPORT void oapiLocalToGlobal (OBJHANDLE hObj, const VECTOR3 *loc, VECTOR3 *glob)
{
	if (((Body*)hObj)->s0) {
		Vector vglob;
		((Body*)hObj)->LocalToGlobal(Vector(loc->x, loc->y, loc->z), vglob);
		glob->x = vglob.x, glob->y = vglob.y, glob->z = vglob.z;
	}
}

DLLEXPORT void oapiEquToLocal (OBJHANDLE hObj, double lng, double lat, double rad, VECTOR3 *loc)
{
	if (((Body*)hObj)->s0) {
		Vector vloc;
		((Body*)hObj)->EquatorialToLocal(lng, lat, rad, vloc);
		loc->x = vloc.x, loc->y = vloc.y, loc->z = vloc.z;
	}
}

DLLEXPORT void oapiLocalToEqu (OBJHANDLE hObj, const VECTOR3 &loc, double *lng, double *lat, double *rad)
{
	if (((Body*)hObj)->s0) {
		Vector vloc(loc.x, loc.y, loc.z);
		((Body*)hObj)->LocalToEquatorial(vloc, *lng, *lat, *rad);
	}
}

DLLEXPORT void oapiEquToGlobal (OBJHANDLE hObj, double lng, double lat, double rad, VECTOR3 *glob)
{
	if (((Body*)hObj)->s0) {
		Vector vglob;
		((Body*)hObj)->EquatorialToGlobal(lng, lat, rad, vglob);
		glob->x = vglob.x, glob->y = vglob.y, glob->z = vglob.z;
	}
}

DLLEXPORT void oapiGlobalToEqu (OBJHANDLE hObj, const VECTOR3 &glob, double *lng, double *lat, double *rad)
{
	if (((Body*)hObj)->s0) {
		Vector vglob(glob.x, glob.y, glob.z);
		((Body*)hObj)->GlobalToEquatorial(vglob, *lng, *lat, *rad);
	}
}

DLLEXPORT double oapiOrthodome (double lng1, double lat1, double lng2, double lat2)
{
	return Orthodome (lng1, lat1, lng2, lat2);
}



DLLEXPORT BOOL oapiGetPitch (OBJHANDLE hVessel, double *pitch)
{
	if ((Vessel*)hVessel) {
		const SurfParam *sp = ((Vessel*)hVessel)->GetSurfParam();
		if (sp) {
			*pitch = sp->pitch;
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetBank (OBJHANDLE hVessel, double *bank)
{
	if ((Vessel*)hVessel) {
		const SurfParam *sp = ((Vessel*)hVessel)->GetSurfParam();
		if (sp) {
			*bank = sp->bank;
			return TRUE;
		}
		else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}


DLLEXPORT BOOL oapiGetEquPos (OBJHANDLE hVessel, double *longitude, double *latitude, double *radius)
{
	if ((Vessel*)hVessel) {
		const SurfParam *sp = ((Vessel*)hVessel)->GetSurfParam();
		if (sp) {
			*longitude = sp->lng;
			*latitude  = sp->lat;
			*radius    = sp->rad;
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusAltitude (double *alt)
{
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		*alt = sp->alt;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusPitch (double *pitch)
{
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		*pitch = sp->pitch;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusBank (double *bank)
{
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		*bank = sp->bank;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusHeading (double *heading)
{
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		*heading = sp->dir;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusEquPos (double *longitude, double *latitude, double *radius)
{
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		*longitude = sp->lng;
		*latitude  = sp->lat;
		*radius    = sp->rad;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetGroundspeed (OBJHANDLE hVessel, double *groundspeed)
{
	Vessel *v = (hVessel ? (Vessel*)hVessel : g_focusobj);
	const SurfParam *sp = v->GetSurfParam();
	if (sp) {
		*groundspeed = sp->groundspd;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT bool oapiGetGroundspeedVector (OBJHANDLE hVessel, REFFRAME frame, VECTOR3 *vel)
{
	Vessel *v = (hVessel ? (Vessel*)hVessel : g_focusobj);
	const SurfParam *sp = v->GetSurfParam();
	if (sp) {
		switch (frame) {
		case FRAME_GLOBAL:
			vel->x = sp->groundvel_glob.x, vel->y = sp->groundvel_glob.y, vel->z = sp->groundvel_glob.z;
			return true;
		case FRAME_LOCAL:
			vel->x = sp->groundvel_ship.x, vel->y = sp->groundvel_ship.y, vel->z = sp->groundvel_ship.z;
			return true;
		case FRAME_REFLOCAL: {
			Vector hvel (tmul (sp->ref->GRot(), sp->groundvel_glob));
			vel->x = hvel.x, vel->y = hvel.y, vel->z = hvel.z;
			} return true;
		case FRAME_HORIZON: {
			Vector hvel (tmul (sp->ref->GRot(), sp->groundvel_glob));
			hvel.Set (mul (sp->L2H, hvel));
			vel->x = hvel.x, vel->y = hvel.y, vel->z = hvel.z;
			} return true;
		default:
			vel->x = vel->y = vel->z = 0.0;
			return false;
		}
	} else {
		vel->x = vel->y = vel->z = 0.0;
		return false;
	}
}

DLLEXPORT BOOL oapiGetAirspeed (OBJHANDLE hVessel, double *airspeed)
{
	Vessel *v = (hVessel ? (Vessel*)hVessel : g_focusobj);
	const SurfParam *sp = v->GetSurfParam();
	if (sp) {
		*airspeed = sp->airspd;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT bool oapiGetAirspeedVector (OBJHANDLE hVessel, REFFRAME frame, VECTOR3 *vel)
{
	Vessel *v = (hVessel ? (Vessel*)hVessel : g_focusobj);
	const SurfParam *sp = v->GetSurfParam();
	if (sp) {
		switch (frame) {
		case FRAME_GLOBAL:
			vel->x = sp->airvel_glob.x, vel->y = sp->airvel_glob.y, vel->z = sp->airvel_glob.z;
			return true;
		case FRAME_LOCAL:
			vel->x = sp->airvel_ship.x, vel->y = sp->airvel_ship.y, vel->z = sp->airvel_ship.z;
			return true;
		case FRAME_REFLOCAL: {
			Vector hvel (tmul (sp->ref->GRot(), sp->airvel_glob));
			vel->x = hvel.x, vel->y = hvel.y, vel->z = hvel.z;
			} return true;
		case FRAME_HORIZON: {
			Vector hvel (tmul (sp->ref->GRot(), sp->airvel_glob));
			hvel.Set (mul (sp->L2H, hvel));
			vel->x = hvel.x, vel->y = hvel.y, vel->z = hvel.z;
			} return true;
		default:
			vel->x = vel->y = vel->z = 0.0;
			return false;
		}
	} else {
		vel->x = vel->y = vel->z = 0.0;
		return false;
	}
}

DLLEXPORT BOOL oapiGetAirspeedVector (OBJHANDLE hVessel, VECTOR3 *speedvec)
{
	LOGOUT_OBSOLETE;
	const SurfParam *sp = ((Vessel*)hVessel)->GetSurfParam();
	if (sp) {
		Vector hvel (tmul (sp->ref->GRot(), sp->airvel_glob));
		hvel.Set (mul (sp->L2H, hvel));
		speedvec->x = hvel.x, speedvec->y = hvel.y, speedvec->z = hvel.z;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetShipAirspeedVector (OBJHANDLE hVessel, VECTOR3 *speedvec)
{
	LOGOUT_OBSOLETE;
	const SurfParam *sp = ((Vessel*)hVessel)->GetSurfParam();
	if (sp) {
		speedvec->x = sp->airvel_ship.x, speedvec->y = sp->airvel_ship.y, speedvec->z = sp->airvel_ship.z;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusAirspeed (double *airspeed)
{
	LOGOUT_OBSOLETE;
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		*airspeed = sp->airspd;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusAirspeedVector (VECTOR3 *speedvec)
{
	LOGOUT_OBSOLETE;
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		Vector hvel (tmul (sp->ref->GRot(), sp->airvel_glob));
		hvel.Set (mul (sp->L2H, hvel));
		speedvec->x = hvel.x, speedvec->y = hvel.y, speedvec->z = hvel.z;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetFocusShipAirspeedVector (VECTOR3 *speedvec)
{
	LOGOUT_OBSOLETE;
	const SurfParam *sp = g_focusobj->GetSurfParam();
	if (sp) {
		speedvec->x = sp->airvel_ship.x, speedvec->y = sp->airvel_ship.y, speedvec->z = sp->airvel_ship.z;
		return TRUE;
	} else {
		return FALSE;
	}
}

DLLEXPORT void oapiGetAtm (OBJHANDLE hVessel, ATMPARAM *prm, OBJHANDLE *hAtmRef)
{
	Vessel *v = (hVessel ? (Vessel*)hVessel : g_focusobj);
	bool bAtm;

	if (!(bAtm = v->AtmPressureAndDensity (prm->p, prm->rho)))
		prm->p = prm->rho = 0.0;
	if (!v->AtmTemperature (prm->T))
		prm->T = 0.0;

	if (hAtmRef)
		*hAtmRef = (bAtm ? (OBJHANDLE)v->ProxyBody() : NULL);
}

DLLEXPORT void oapiGetAtmPressureDensity (OBJHANDLE hVessel, double *pressure, double *density)
{
	static bool bWarning = true;
	if (bWarning) {
		LogOut_Obsolete(__FUNCTION__);
		bWarning = false;
	}
	if (!((Vessel*)hVessel)->AtmPressureAndDensity (*pressure, *density))
		*pressure = *density = 0.0;
}

DLLEXPORT void oapiGetFocusAtmPressureDensity (double *pressure, double *density)
{
	static bool bWarning = true;
	if (bWarning) {
		LogOut_Obsolete(__FUNCTION__);
		bWarning = false;
	}
	if (!g_focusobj->AtmPressureAndDensity (*pressure, *density))
		*pressure = *density = 0.0;
}

DLLEXPORT VECTOR3 oapiGetGroundVector (OBJHANDLE hPlanet, double lng, double lat, int frame)
{
	return MakeVECTOR3 (((Planet*)hPlanet)->GroundVelocity (lng, lat, 0, frame));
}

DLLEXPORT VECTOR3 oapiGetWindVector (OBJHANDLE hPlanet, double lng, double lat, double alt, int frame, double *windspeed)
{
	return MakeVECTOR3 (((Planet*)hPlanet)->WindVelocity (lng, lat, alt, frame, 0, windspeed));
}

// ===========================================================================
// Aerodynamics helper functions

DLLEXPORT double oapiGetInducedDrag (double cl, double A, double eps)
{
	return (cl*cl)/(Pi*A*eps);
}


// ===========================================================================

DLLEXPORT void oapiGetEngineStatus (OBJHANDLE hVessel, ENGINESTATUS *es)
{
	((Vessel*)hVessel)->EngineStatus (es);
}

DLLEXPORT void oapiGetFocusEngineStatus (ENGINESTATUS *es)
{
	g_focusobj->EngineStatus (es);
}

DLLEXPORT void oapiSetEngineLevel (OBJHANDLE hVessel, ENGINETYPE engine, double level)
{
	if (((Body*)hVessel)->Type() == OBJTP_VESSEL) {
		Vessel *vessel = (Vessel*)hVessel;
		switch (engine) {
		case ENGINE_MAIN:
			//vessel->SetThrustMain (level);
			vessel->SetThrusterGroupLevel (THGROUP_MAIN, level);
			break;
		case ENGINE_RETRO:
			//vessel->SetThrustMain (-level);
			vessel->SetThrusterGroupLevel (THGROUP_RETRO, level);
			break;
		case ENGINE_HOVER:
			//vessel->SetThrustHover (level);
			vessel->SetThrusterGroupLevel (THGROUP_HOVER, level);
			break;
		}
	}
}




DLLEXPORT int oapiGetAttitudeMode (OBJHANDLE hVessel)
{
	return ((Vessel*)hVessel)->AttMode();
}

DLLEXPORT int oapiToggleAttitudeMode (OBJHANDLE hVessel)
{
	return ((Vessel*)hVessel)->ToggleAttMode();
}

DLLEXPORT bool oapiSetAttitudeMode (OBJHANDLE hVessel, int mode)
{
	return ((Vessel*)hVessel)->SetAttMode (mode);
}

DLLEXPORT int oapiGetFocusAttitudeMode ()
{
	return g_focusobj->AttMode();
}

DLLEXPORT int oapiToggleFocusAttitudeMode ()
{
	return g_focusobj->ToggleAttMode();
}

DLLEXPORT bool oapiSetFocusAttitudeMode (int mode)
{
	return g_focusobj->SetAttMode (mode);
}

DLLEXPORT void oapiSetShowGrapplePoints (bool show)
{
	g_bShowGrapple = show;
}

DLLEXPORT bool oapiGetShowGrapplePoints ()
{
	return g_bShowGrapple;
}






DLLEXPORT double oapiGetSysMJD ()
{
	return MJD (time (NULL)) + UTC_CT_diff*day;
}







DLLEXPORT void oapiSetPause (bool pause)
{
	g_pOrbiter->Pause (pause == true);
}


// Functions for planetary bodies






DLLEXPORT bool oapiPlanetHasAtmosphere (OBJHANDLE hPlanet)
{
	return ((Planet*)hPlanet)->HasAtmosphere();
}

DLLEXPORT void oapiGetPlanetAtmParams (OBJHANDLE hPlanet, double rad, ATMPARAM *prm)
{
	// This should operate directly on altitude
	Planet *planet = (Planet*)hPlanet;
	planet->GetAtmParam (rad - planet->Size(), 0, 0, prm);
}

DLLEXPORT void oapiGetPlanetAtmParams (OBJHANDLE hPlanet, double alt, double lng, double lat, ATMPARAM *prm)
{
	Planet *planet = (Planet*)hPlanet;
	planet->GetAtmParam (alt, lng, lat, prm);
}

DLLEXPORT const ATMCONST *oapiGetPlanetAtmConstants (OBJHANDLE hPlanet)
{
	return ((Planet*)hPlanet)->AtmParams ();
}



// Elevation support interface
DLLEXPORT ELEVHANDLE oapiElevationManager (OBJHANDLE hPlanet)
{
	Body *body = (Body*)hPlanet;
	if (body->Type() != OBJTP_PLANET) return 0;
	Planet *planet = (Planet*)body;
	return (ELEVHANDLE)planet->ElevMgr();
}

DLLEXPORT double oapiSurfaceElevation (OBJHANDLE hPlanet, double lng, double lat)
{
	Body *body = (Body*)hPlanet;
	if (body->Type() != OBJTP_PLANET) return 0.0;
	Planet *planet = (Planet*)body;
	ElevationManager *emgr = planet->ElevMgr();
	return (emgr ? emgr->Elevation (lat, lng) : 0.0);
}

DLLEXPORT double oapiSurfaceElevationEx(OBJHANDLE hPlanet, double lng, double lat, int tgtlvl, std::vector<ElevationTile> *tilecache, VECTOR3 *nml, int *lvl)
{
	Body *body = (Body*)hPlanet;
	if (body->Type() != OBJTP_PLANET) return 0.0;
	Planet *planet = (Planet*)body;
	ElevationManager *emgr = planet->ElevMgr();
	Vector normal;
	if (nml)
		normal = MakeVector(*nml);
	double elev = (emgr ? emgr->Elevation(lat, lng, tgtlvl, tilecache, nml ? &normal : 0, lvl) : 0.0);
	if (nml)
		*nml = MakeVECTOR3(normal);
	return elev;
}

DLLEXPORT std::vector<ElevationTile> *InitTileCache(int size)
{
	return new std::vector<ElevationTile>(size);
}

DLLEXPORT void ReleaseTileCache(std::vector<ElevationTile> *tilecache)
{
	delete tilecache;
}

// Surface base interface

DLLEXPORT void oapiGetBaseEquPos (OBJHANDLE hBase, double *lng, double *lat, double *rad)
{
	Base *base = (Base*)hBase;
	base->EquPos (*lng, *lat);
	if (rad) *rad = base->RefPlanet()->Size(); // for now
}

DLLEXPORT DWORD oapiGetBasePadCount (OBJHANDLE hBase)
{
	return ((Base*)hBase)->nPad();
}

DLLEXPORT bool oapiGetBasePadEquPos (OBJHANDLE hBase, DWORD pad, double *lng, double *lat, double *rad)
{
	Base *base = (Base*)hBase;
	if (pad >= base->nPad()) return false;
	base->Pad_EquPos (pad, *lng, *lat);
	if (rad) *rad = base->RefPlanet()->Size(); // for now
	return true;
}

DLLEXPORT bool oapiGetBasePadStatus (OBJHANDLE hBase, DWORD pad, int *status)
{
	Base *base = (Base*)hBase;
	if (pad >= base->nPad()) return false;
	*status = base->PadStatus (pad)->status;
	return true;
}

DLLEXPORT NAVHANDLE oapiGetBasePadNav (OBJHANDLE hBase, DWORD pad)
{
	Base *base = (Base*)hBase;
	if (pad >= base->nPad()) return 0;
	return (NAVHANDLE)base->PadStatus (pad)->nav;
}

// Navigation radio transmitter functions

DLLEXPORT void oapiGetNavPos (NAVHANDLE hNav, VECTOR3 *gpos)
{
	Vector p;
	((Nav*)hNav)->GPos (p);
	gpos->x = p.x;
	gpos->y = p.y;
	gpos->z = p.z;
}

DLLEXPORT DWORD oapiGetNavChannel (NAVHANDLE hNav)
{
	return ((Nav*)hNav)->GetStep();
}

DLLEXPORT float oapiGetNavFreq (NAVHANDLE hNav)
{
	return ((Nav*)hNav)->GetFreq();
}

DLLEXPORT double oapiGetNavSignal (NAVHANDLE hNav, const VECTOR3 &gpos)
{
	return ((Nav*)hNav)->FieldStrength (MakeVector(gpos));
}

DLLEXPORT float oapiGetNavRange (NAVHANDLE hNav)
{
	return ((Nav*)hNav)->GetRange();
}

DLLEXPORT DWORD oapiGetNavType (NAVHANDLE hNav)
{
	return (DWORD)((Nav*)hNav)->Type();
}

DLLEXPORT int oapiGetNavData (NAVHANDLE hNav, NAVDATA *data)
{
	Nav *nav = (Nav*)hNav;
	nav->GetData (data);
	return 0;
}

DLLEXPORT int oapiGetNavDescr (NAVHANDLE hNav, char *descr, int maxlen)
{
	return ((Nav*)hNav)->IdString (descr, maxlen);
}

DLLEXPORT bool oapiNavInRange (NAVHANDLE hNav, const VECTOR3 &gpos)
{
	Vector p(gpos.x, gpos.y, gpos.z);
	return ((Nav*)hNav)->InRange (p);
}

DLLEXPORT INTERPRETERHANDLE oapiCreateInterpreter ()
{
	return g_pOrbiter->Script()->NewInterpreter();
}

DLLEXPORT int oapiDelInterpreter (INTERPRETERHANDLE hInterp)
{
	return g_pOrbiter->Script()->DelInterpreter (hInterp);
}

DLLEXPORT bool oapiExecScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd)
{
	return g_pOrbiter->Script()->ExecScriptCmd (hInterp, cmd);
}

DLLEXPORT bool oapiAsyncScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd)
{
	return g_pOrbiter->Script()->AsyncScriptCmd (hInterp, cmd);
}

DLLEXPORT lua_State *oapiGetLua (INTERPRETERHANDLE hInterp)
{
	return g_pOrbiter->Script()->GetLua (hInterp);
}

DLLEXPORT VISHANDLE *oapiObjectVisualPtr (OBJHANDLE hObject)
{
	return (VISHANDLE*)((Body*)hObject)->GetVishandlePtr();
}







DLLEXPORT void oapiParticleSetLevelRef (PSTREAM_HANDLE ph, double *lvl)
{
	((oapi::ParticleStream*)ph)->SetLevelPtr (lvl);
}

DLLEXPORT DWORD oapiMeshTextureCount (MESHHANDLE hMesh)
{
	return ((Mesh*)hMesh)->nTexture();
}





















DLLEXPORT int oapiMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp mat, oapi::FVECTOR4 *out)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkMeshMaterialEx(hMesh, matidx, mat, out) : 1);
}


DLLEXPORT int oapiSetMaterial (DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL *mat)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkSetMeshMaterial (hMesh, matidx, mat) : 1);
}

DLLEXPORT DWORD oapiAddMaterial (MESHHANDLE hMesh, MATERIAL *mat)
{
	D3DMATERIAL7 *m = (D3DMATERIAL7*)mat;
	return ((Mesh*)hMesh)->AddMaterial (*m);
}

DLLEXPORT bool oapiDeleteMaterial (MESHHANDLE hMesh, DWORD idx)
{
	return ((Mesh*)hMesh)->DeleteMaterial (idx);
}

DLLEXPORT bool oapiSetMeshProperty (MESHHANDLE hMesh, DWORD property, DWORD value)
{
	Mesh *mesh = (Mesh*)hMesh;
	switch (property) {
	case MESHPROPERTY_MODULATEMATALPHA:
		mesh->EnableMatAlpha (value != 0);
		return true;
	}
	return false;
}

DLLEXPORT bool oapiSetMeshProperty (DEVMESHHANDLE hMesh, DWORD property, DWORD value)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkSetMeshProperty (hMesh, property, value) : false);
}





DLLEXPORT void oapiToggleHUDColour ()
{
	if (g_pane) g_pane->ToggleHUDColour ();
}

DLLEXPORT double oapiGetHUDIntensity ()
{
	return (g_pane ? g_pane->HudIntens() : 0.0);
}

DLLEXPORT void oapiSetHUDIntensity (double val)
{
	if (g_pane) g_pane->SetHUDIntens (val);
}

DLLEXPORT void oapiIncHUDIntensity ()
{
	if (g_pane) g_pane->IncHUDIntens ();
}

DLLEXPORT void oapiDecHUDIntensity ()
{
	if (g_pane) g_pane->DecHUDIntens ();
}

DLLEXPORT void oapiRenderHUD (MESHHANDLE hMesh, SURFHANDLE *hTex)
{
	if (g_pane) g_pane->RenderCustomHUD (hMesh, hTex);
}

DLLEXPORT void oapiOpenMFD (int mode, int mfd)
{
	if (g_pane) g_pane->OpenMFD (mfd, mode);
}


DLLEXPORT int oapiRegisterMFDMode (MFDMODESPEC &spec)
{
	LOGOUT_OBSOLETE;
	return Instrument::RegisterUserMode (&spec);
}

DLLEXPORT int oapiRegisterMFDMode (MFDMODESPECEX &spec)
{
	return Instrument::RegisterUserMode (&spec);
}

DLLEXPORT bool oapiUnregisterMFDMode (int mode)
{
	return Instrument::UnregisterUserMode (mode);
}


DLLEXPORT double oapiSetMFDRefreshIntervalMultiplier (int mfd, double multiplier)
{
	return g_pane->SetMFDRefreshIntervalMultiplier (mfd, multiplier);
}

DLLEXPORT int oapiBroadcastMFDMessage (int mfdmode, int msg, void *data)
{
	return g_pane->BroadcastMFDMessage (mfdmode, msg, data);
}


DLLEXPORT void oapiRefreshMFDButtons (int mfd, OBJHANDLE hVessel)
{
	if (!hVessel || (Vessel*)hVessel == g_focusobj)
		g_focusobj->MFDchanged (mfd, MFD_REFRESHBUTTONS);
}





DLLEXPORT void oapiRegisterExternMFD (ExternMFD *emfd, const MFDSPEC &spec)
{
	if (g_pane)
		g_pane->RegisterExternMFD (emfd, spec);
}

DLLEXPORT bool oapiUnregisterExternMFD (ExternMFD *emfd)
{
	return (g_pane ? g_pane->UnregisterExternMFD (emfd) : false);
}

DLLEXPORT void oapiDisableMFDMode (int mode)
{
	Instrument::DisableMode (mode);
	if (g_pane)
		g_pane->MFDModeDisabled (mode);
}

DLLEXPORT int oapiGetMFDModeSpecEx (char *name, MFDMODESPECEX **spec)
{
	return Instrument::ModeFromName (name, spec);
}

DLLEXPORT int oapiGetMFDModeSpec (char *name, MFDMODESPEC **spec)
{
	return Instrument::ModeFromNameOld (name, spec);
}

DLLEXPORT void oapiVCRegisterMFD (int mfd, const VCMFDSPEC *spec)
{
	g_pane->RegisterVCMFD (mfd, spec);
}

DLLEXPORT void oapiVCRegisterHUD (const VCHUDSPEC *spec)
{
	if (g_pane)
		g_pane->RegisterVCHUD (spec);
}





DLLEXPORT void oapiTriggerRedrawArea (int panel_id, int vc_id, int area_id)
{
	static bool bwarn = true;
	if (bwarn) {
		LogOut_Obsolete (__FUNCTION__, "Replaced by VESSEL::TriggerRedrawArea");
		bwarn = false;
	}
	if (g_pane)
		g_pane->TriggerRedrawArea (panel_id, vc_id, area_id);
}

DLLEXPORT void oapiTriggerPanelRedrawArea (int panel_id, int area_id)
{
	static bool bwarn = true;
	if (bwarn) {
		LogOut_Obsolete (__FUNCTION__, "Replaced by VESSEL::TriggerPanelRedrawArea");
		bwarn = false;
	}
	if (g_pane)
		g_pane->TriggerPanelRedrawArea (panel_id, area_id);
}

DLLEXPORT bool oapiBltPanelAreaBackground (int area_id, SURFHANDLE surf)
{
	if (g_pane)
		return g_pane->BltPanelAreaBackground (area_id, surf);
	else
		return false;
}

DLLEXPORT void oapiSetDefNavDisplay (int mode)
{
	DefaultPanel *defpanel = g_pane->GetDefaultPanel();
	if (defpanel) defpanel->SetNavDisplayMode (mode);
}

DLLEXPORT void oapiSetDefRCSDisplay (int mode)
{
	DefaultPanel *defpanel = g_pane->GetDefaultPanel();
	if (defpanel) defpanel->SetRCSDisplayMode (mode);
}

DLLEXPORT int oapiSwitchPanel (int direction)
{
	return g_pane->SwitchPanel (direction);
}


DLLEXPORT void oapiSetPanelBlink (VECTOR3 v[4])
{
	g_pane->SetPanel2DBlink (v);
}

DLLEXPORT void oapiVCTriggerRedrawArea (int vc_id, int area_id)
{
	g_pane->TriggerVCRedrawArea (vc_id, area_id);
}









/**
	* \brief Create a Font
	* \param height Font height
	* \param face Name of the font
	* \param width Width of the font (0 for default aspect ration)
	* \param weight Font thikness (400 for default weight)
	* \param style A combination of \see FontStyle flags (0 for default)
	* \param spacing A spacing between charters in a string (0.0f for default)
	* \return A pointer to a created or pre-existing font or NULL in a case of an error.
	*/







DLLEXPORT HDC oapiGetDC (SURFHANDLE surf)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	HDC hDC = NULL;
	if (gc && surf)
		hDC = gc->clbkGetSurfaceDC (surf);
	return hDC;
}

DLLEXPORT void oapiReleaseDC (SURFHANDLE surf, HDC hDC)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc && surf && hDC)
		gc->clbkReleaseSurfaceDC (surf, hDC);
}

DLLEXPORT SURFHANDLE oapiCreateSurface (HBITMAP hBmp, bool release_bmp)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	SURFHANDLE surf = NULL;
	if (gc) surf = gc->clbkCreateSurface (hBmp);
	if (release_bmp) DeleteObject ((HGDIOBJ)hBmp);
	return surf;
}





DLLEXPORT void oapiClearSurfaceColourKey (SURFHANDLE surf)
{
	if (!surf) return;
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	//if (gc) gc->clbClearSurfaceColourKey (surf); // TODO
	((LPDIRECTDRAWSURFACE7)surf)->SetColorKey (DDCKEY_SRCBLT, 0);
}







DLLEXPORT bool oapiAcceptDelayedKey (char key, double interval)
{
	static char lastkey = (char)255;
	static double lastkeytime = 0.0;

	if ((key == lastkey) && td.SysT0-lastkeytime < interval && td.SysT0 > lastkeytime)
		return false;
	lastkey = key;
	lastkeytime = td.SysT0;
	return true;
}

DLLEXPORT LAUNCHPADITEM_HANDLE oapiRegisterLaunchpadItem (LaunchpadItem *item, LAUNCHPADITEM_HANDLE parent)
{
	return (LAUNCHPADITEM_HANDLE)g_pOrbiter->Launchpad()->RegisterExtraParam (item, (HTREEITEM)parent);
}

DLLEXPORT bool oapiUnregisterLaunchpadItem (LaunchpadItem *item)
{
	return g_pOrbiter->Launchpad()->UnregisterExtraParam (item);
}

DLLEXPORT LAUNCHPADITEM_HANDLE oapiFindLaunchpadItem (const char *name, LAUNCHPADITEM_HANDLE parent)
{
	return g_pOrbiter->Launchpad()->FindExtraParam (name, (HTREEITEM)parent);
}

DLLEXPORT DWORD oapiRegisterCustomCmd (char *label, char *desc, CustomFunc func, void *context)
{
	return g_pOrbiter->RegisterCustomCmd (label, desc, func, context);
}

DLLEXPORT bool oapiUnregisterCustomCmd (int cmdId)
{
	return g_pOrbiter->UnregisterCustomCmd (cmdId);
}

DLLEXPORT int oapiRegisterCustomMenuCmd (const char *label, const char *imagepath, CustomFunc func, void *context)
{
	return g_pOrbiter->RegisterMenuCmd (label, imagepath, func, context);
}

DLLEXPORT void oapiUnregisterCustomMenuCmd (int cmdId)
{
	return g_pOrbiter->UnregisterMenuCmd (cmdId);
}

DLLEXPORT HWND oapiOpenDialog (HINSTANCE hDLLInst, int resourceId, DLGPROC msgProc, void *context)
{
	return g_pOrbiter->OpenDialog (hDLLInst, resourceId, msgProc, context);
}

DLLEXPORT HWND oapiOpenDialogEx (HINSTANCE hDLLInst, int resourceId, DLGPROC msgProc, DWORD flag, void *context)
{
	return g_pOrbiter->OpenDialogEx (hDLLInst, resourceId, msgProc, flag, context);
}

DLLEXPORT void oapiOpenDialog(ImGuiDialog *e)
{
	g_pOrbiter->DlgMgr()->AddEntry(e);
	e->Activate();
}

DLLEXPORT HWND oapiFindDialog (HINSTANCE hDLLInst, int resourceId)
{
	return g_pOrbiter->IsDialog (hDLLInst, resourceId);
}

DLLEXPORT void oapiCloseDialog (HWND hDlg)
{
	g_pOrbiter->CloseDialog (hDlg);
}

DLLEXPORT void oapiCloseDialog(ImGuiDialog *e)
{
	DialogManager *dmgr = g_pOrbiter->DlgMgr();
	if(dmgr)
		dmgr->DelEntry(e);
}

DLLEXPORT void *oapiGetDialogContext (HWND hDlg)
{
	DialogManager *dlgmgr = g_pOrbiter->DlgMgr();
	return (dlgmgr ? dlgmgr->GetDialogContext (hDlg) : NULL);
}

DLLEXPORT bool oapiRegisterWindow (HINSTANCE hDLLInst, HWND hWnd, DWORD flag)
{
	return g_pOrbiter->RegisterWindow (hDLLInst, hWnd, flag); 
}

DLLEXPORT bool oapiAddTitleButton (DWORD msgid, HBITMAP hBmp, DWORD flag)
{
	DialogManager *dlgmgr = g_pOrbiter->DlgMgr();
	return (dlgmgr ? dlgmgr->AddTitleButton (msgid, hBmp, flag) : false);
}

DLLEXPORT DWORD oapiGetTitleButtonState (HWND hDlg, DWORD msgid)
{
	DialogManager *dlgmgr = g_pOrbiter->DlgMgr();
	return (dlgmgr ? dlgmgr->GetTitleButtonState (hDlg, msgid) : 0);
}

DLLEXPORT bool oapiSetTitleButtonState (HWND hDlg, DWORD msgid, DWORD state)
{
	DialogManager *dlgmgr = g_pOrbiter->DlgMgr();
	return (dlgmgr ? dlgmgr->SetTitleButtonState (hDlg, msgid, state) : false);
}

DLLEXPORT INT_PTR oapiDefDialogProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return OrbiterDefDialogProc (hDlg, uMsg, wParam, lParam);
}

DLLEXPORT bool oapiOpenHelp (HELPCONTEXT *hcontext)
{
	g_pOrbiter->OpenHelp (hcontext);
	return true;
}

DLLEXPORT bool oapiOpenLaunchpadHelp (HELPCONTEXT *hcontext)
{
	g_pOrbiter->OpenLaunchpadHelp (hcontext);
	return true;
}

DLLEXPORT DWORD oapiGetMainMenuVisibilityMode()
{
	CFG_UIPRM &prm = g_pOrbiter->Cfg()->CfgUIPrm;
	return prm.MenuMode;
}

DLLEXPORT void oapiSetMainMenuVisibilityMode (DWORD mode)
{
	CFG_UIPRM &prm = g_pOrbiter->Cfg()->CfgUIPrm;
	prm.MenuMode = mode;
}

DLLEXPORT DWORD oapiGetMainInfoVisibilityMode()
{
	CFG_UIPRM &prm = g_pOrbiter->Cfg()->CfgUIPrm;
	return prm.InfoMode;
}

DLLEXPORT void oapiSetMainInfoVisibilityMode (DWORD mode)
{
	CFG_UIPRM &prm = g_pOrbiter->Cfg()->CfgUIPrm;
	prm.InfoMode = mode;
}



DLLEXPORT void oapiExitOrbiter(int code)
{
	exit(code);
}



DLLEXPORT void oapiOpenInputBox (char *title, bool (*Clbk)(void*,char*,void*), char *buf, int vislen, void *usrdata)
{
	g_input->Open (title, buf, vislen, (InputBox::Callbk)Clbk, usrdata);
}

DLLEXPORT void oapiOpenInputBoxEx (const char *title, bool (*Clbk_enter)(void*,char*,void*), bool (*Clbk_cancel)(void*,char*,void*),
	char *buf, int vislen, void *usrdata, DWORD flags)
{
	g_input->OpenEx (title, buf, vislen, (InputBox::Callbk)Clbk_enter, (InputBox::Callbk)Clbk_cancel, usrdata, flags);
}

DLLEXPORT bool oapiSimulateBufferedKey (DWORD key, DWORD *mod, DWORD nmod, bool onRunningOnly)
{
	return g_pOrbiter->SendKbdBuffered (key, mod, nmod, onRunningOnly);
}

DLLEXPORT bool oapiSimulateImmediateKey (char kstate[256], bool onRunningOnly)
{
	return g_pOrbiter->SendKbdImmediate (kstate, onRunningOnly);
}

DLLEXPORT const char *oapiGetKeyState ()
{
	return g_pOrbiter->KeyState();
}

DLLEXPORT NOTEHANDLE oapiCreateAnnotation (bool exclusive, double size, const VECTOR3 &col)
{
	COLORREF c = ((int)(col.z*255.99) << 16) + ((int)(col.y*255.99) << 8) + (int)(col.x*255.99);
	return (NOTEHANDLE)g_pOrbiter->CreateAnnotation (exclusive, size, c);
}

DLLEXPORT void oapiAnnotationSetPos (NOTEHANDLE hNote, double x1, double y1, double x2, double y2)
{
	if (hNote) {
		oapi::ScreenAnnotation *sn = (oapi::ScreenAnnotation*)hNote;
		sn->SetPosition (x1, y1, x2, y2);
	}
}

DLLEXPORT void oapiAnnotationSetSize (NOTEHANDLE hNote, double size)
{
	if (hNote) {
		oapi::ScreenAnnotation *sn = (oapi::ScreenAnnotation*)hNote;
		sn->SetSize (size);
	}
}

DLLEXPORT void oapiAnnotationSetColour (NOTEHANDLE hNote, const VECTOR3 &col)
{
	if (hNote) {
		oapi::ScreenAnnotation *sn = (oapi::ScreenAnnotation*)hNote;
		sn->SetColour (col);
	}
}

DLLEXPORT void oapiAnnotationSetText (NOTEHANDLE hNote, char *note)
{
	if (hNote) {
		oapi::ScreenAnnotation *sn = (oapi::ScreenAnnotation*)hNote;
		if (note) sn->SetText (note);
		else sn->ClearText();
	}
}

DLLEXPORT bool oapiDelAnnotation (NOTEHANDLE hNote)
{
	if (hNote)
		return g_pOrbiter->DeleteAnnotation ((oapi::ScreenAnnotation*)hNote);
	else
		return false;
}



DLLEXPORT double oapiRand ()
{
	static double irmax = 1.0/(double)RAND_MAX;
	return (double)rand() * irmax;
}

DLLEXPORT DWORD oapiDeflate (const BYTE *inp, DWORD ninp, BYTE *outp, DWORD noutp)
{
	int ret, ndata;
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

	strm.avail_in = ninp;
	strm.next_in = (BYTE*)inp;

	strm.avail_out = noutp;
	strm.next_out = outp;
	ret = deflate(&strm, Z_FINISH);
	ndata = (ret == Z_STREAM_END ? noutp - strm.avail_out : 0);

	deflateEnd(&strm);
	return ndata;
}

DLLEXPORT DWORD oapiInflate (const BYTE *inp, DWORD ninp, BYTE *outp, DWORD noutp)
{
	DWORD ndata = noutp;
	if (uncompress (outp, &ndata, inp, ninp) != Z_OK)
		return 0;
	return ndata;
}

// ------------------------------------------------------------------------------
// Undocumented interface functions
// ------------------------------------------------------------------------------

DLLEXPORT void WriteScenario_state (FILEHANDLE f, char *tag, const AnimState &s)
{
	char cbuf[256];
	sprintf (cbuf, "%d %0.4f", s.action-1, s.pos);
	oapiWriteScenario_string (f, tag, cbuf);
}

DLLEXPORT void sscan_state (char *str, AnimState &s)
{
	int a;
	double p;
	sscanf (str, "%d%lf", &a, &p);
	s.action = (AnimState::Action)(a+1);
	s.pos = p;
}
