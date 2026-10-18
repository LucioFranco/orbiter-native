// Copyright (c) Martin Schweiger
// Licensed under the MIT License
// Shared world and simulation SDK entry points for every application host.
#include "Orbiter.h"
#include "Astro.h"
#include "Psys.h"
#include "Vessel.h"
#include "Planet.h"
#include "Base.h"
#include "Log.h"
#include "Util.h"
#include "Orbitersdk.h"

extern Orbiter *g_pOrbiter;
extern PlanetarySystem *g_psys;
extern Vessel *g_focusobj;
extern TimeData td;

DLLEXPORT OBJHANDLE oapiGetObjectByName (char *name)
{
	return (OBJHANDLE)(g_psys ? g_psys->GetObj (name, true) : 0);
}

DLLEXPORT OBJHANDLE oapiGetVesselByName (char *name)
{
	return (OBJHANDLE)(g_psys ? g_psys->GetVessel (name, true) : 0);
}

DLLEXPORT OBJHANDLE oapiGetVesselByIndex (int index)
{
	if (!g_psys || index < 0 || index >= g_psys->nVessel()) return NULL;
	return (OBJHANDLE)g_psys->GetVessel (index);
}

DLLEXPORT DWORD oapiGetVesselCount ()
{
	return (g_psys ? g_psys->nVessel() : 0);
}

DLLEXPORT bool oapiIsVessel (OBJHANDLE hVessel)
{
	return (g_psys ? g_psys->isVessel ((const Vessel*)hVessel) : false);
}

DLLEXPORT OBJHANDLE oapiGetGbodyByName (char *name)
{
	return (g_psys ? g_psys->GetGravObj (name, true) : 0);
}

DLLEXPORT OBJHANDLE oapiGetGbodyByIndex (int index)
{
	return (g_psys ? g_psys->GetGravObj (index) : 0);
}

DLLEXPORT OBJHANDLE oapiGetGbodyParent (OBJHANDLE hBody)
{
	return (OBJHANDLE)((CelestialBody*)hBody)->Primary();
}

DLLEXPORT OBJHANDLE oapiGetGbodyChild (OBJHANDLE hBody, DWORD index)
{
	CelestialBody* cbody = (CelestialBody*)hBody;
	if (index < cbody->nSecondary())
		return (OBJHANDLE)cbody->Secondary(index);
	else
		return NULL;
}

DLLEXPORT DWORD oapiGetGbodyCount ()
{
	return (g_psys ? g_psys->nGrav() : 0);
}

DLLEXPORT OBJHANDLE oapiGetFocusObject ()
{
	return (OBJHANDLE)g_focusobj;
}

DLLEXPORT OBJHANDLE oapiSetFocusObject (OBJHANDLE hVessel)
{
	Body *body = (Body*)hVessel;
	if (body->Type() != OBJTP_VESSEL) return 0;
	return (OBJHANDLE)g_pOrbiter->SetFocusObject ((Vessel*)hVessel);
}

DLLEXPORT VESSEL *oapiGetVesselInterface (OBJHANDLE hVessel)
{
	if (((Body*)hVessel)->Type() != OBJTP_VESSEL) return 0;
	else return ((Vessel*)hVessel)->GetModuleInterface();
}

DLLEXPORT CELBODY *oapiGetCelbodyInterface (OBJHANDLE hBody)
{
	return ((CelestialBody*)hBody)->GetModuleInterface();
}

DLLEXPORT VESSEL *oapiGetFocusInterface ()
{
	return g_focusobj->GetModuleInterface();
}

DLLEXPORT double oapiGetSize (OBJHANDLE hObj)
{
	return ((Body*)hObj)->Size();
}

DLLEXPORT double oapiGetMass (OBJHANDLE hObj)
{
	return ((Body*)hObj)->Mass();
}

DLLEXPORT void oapiGetGlobalPos (OBJHANDLE hObj, VECTOR3 *pos)
{
	if (((Body*)hObj)->s0) {
		Vector gp(((Body*)hObj)->GPos());
		pos->x = gp.x, pos->y = gp.y, pos->z = gp.z;
	}
}

DLLEXPORT void oapiGetGlobalVel (OBJHANDLE hObj, VECTOR3 *vel)
{
	if (((Body*)hObj)->s0) {
		Vector gv(((Body*)hObj)->GVel());
		vel->x = gv.x, vel->y = gv.y, vel->z = gv.z;
	}
}

DLLEXPORT void oapiGetRelativePos (OBJHANDLE hObj, OBJHANDLE hRef, VECTOR3 *pos)
{
	if (((Body*)hObj)->s0 && ((Body*)hRef)->s0) {
		Vector dp(((Body*)hObj)->GPos()-((Body*)hRef)->GPos());
		pos->x = dp.x, pos->y = dp.y, pos->z = dp.z;
	}
}

DLLEXPORT void oapiGetRelativeVel (OBJHANDLE hObj, OBJHANDLE hRef, VECTOR3 *vel)
{
	if (((Body*)hObj)->s0 && ((Body*)hRef)->s0) {
		Vector dv(((Body*)hObj)->GVel() - ((Body*)hRef)->GVel());
		vel->x = dv.x, vel->y = dv.y, vel->z = dv.z;
	}
}

DLLEXPORT void oapiGetRotationMatrix (OBJHANDLE hObj, MATRIX3 *mat)
{
	if (((Body*)hObj)->s0) {
		const Matrix& m = ((Body*)hObj)->GRot();
		for (int i = 0; i < 9; i++) mat->data[i] = m.data[i];
	}
}

DLLEXPORT BOOL oapiGetAltitude (OBJHANDLE hVessel, double *alt)
{
	if ((Vessel*)hVessel) {
		const SurfParam *sp = ((Vessel*)hVessel)->GetSurfParam();
		if (sp) {
			*alt = sp->alt0;
			return TRUE;
		}
		else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetAltitude (OBJHANDLE hVessel, AltitudeMode mode, double *alt)
{
	if ((Vessel*)hVessel) {
		const SurfParam* sp = ((Vessel*)hVessel)->GetSurfParam();
		if (sp) {
			*alt = (mode == ALTMODE_MEANRAD ? sp->alt0 : sp->alt);
			return TRUE;
		}
		else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

DLLEXPORT BOOL oapiGetHeading (OBJHANDLE hVessel, double *heading)
{
	if ((Vessel*)hVessel) {
		const SurfParam *sp = ((Vessel*)hVessel)->GetSurfParam();
		if (sp) {
			*heading = sp->dir;
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

DLLEXPORT double oapiGetWaveDrag (double M, double M1, double M2, double M3, double cmax)
{
	if (M < M1) return 0.0;
	if (M < M2) return cmax * (M-M1)/(M2-M1);
	if (M < M3) return cmax;
	return cmax * sqrt ((M3*M3-1.0)/(M*M-1.0));
}

DLLEXPORT double oapiGetSimTime ()
{
	return td.SimT0;
}

DLLEXPORT double oapiGetSimStep ()
{
	return td.SimDT;
}

DLLEXPORT double oapiGetSysTime ()
{
	return td.SysT0;
}

DLLEXPORT double oapiGetSysStep ()
{
	return td.SysDT;
}

DLLEXPORT double oapiGetSimMJD ()
{
	return td.MJD0;
}

DLLEXPORT bool oapiSetSimMJD (double mjd, int pmode)
{
	return g_pOrbiter->Timejump (mjd, pmode);
}

DLLEXPORT double oapiGetFrameRate ()
{
	return td.FPS();
}

DLLEXPORT double oapiTime2MJD (double t)
{
	return td.MJD_ref + Day(t);
}

DLLEXPORT double oapiGetTimeAcceleration ()
{
	return td.Warp();
}

DLLEXPORT void oapiSetTimeAcceleration (double warp)
{
	g_pOrbiter->SetWarpFactor (warp);
}

DLLEXPORT bool oapiGetPause ()
{
	return !g_pOrbiter->IsRunning();
}

DLLEXPORT double oapiGetPlanetPeriod (OBJHANDLE hPlanet)
{
	return ((Planet*)hPlanet)->RotT();
}

DLLEXPORT double oapiGetPlanetObliquity (OBJHANDLE hPlanet)
{
	return ((Planet*)hPlanet)->Obliquity();
}

DLLEXPORT double oapiGetPlanetTheta (OBJHANDLE hPlanet)
{
	return ((Planet*)hPlanet)->EqLng();
}

DLLEXPORT void oapiGetPlanetObliquityMatrix (OBJHANDLE hPlanet, MATRIX3 *mat)
{
	const Matrix m = ((CelestialBody*)hPlanet)->RotObliq();
	for (int i = 0; i < 9; i++) mat->data[i] = m.data[i];
}

DLLEXPORT double oapiGetPlanetCurrentRotation (OBJHANDLE hPlanet)
{
	return ((CelestialBody*)hPlanet)->Rotation();
}

DLLEXPORT DWORD oapiGetPlanetJCoeffCount (OBJHANDLE hPlanet)
{
	return ((CelestialBody*)hPlanet)->nJcoeff();
}

DLLEXPORT double oapiGetPlanetJCoeff (OBJHANDLE hPlanet, DWORD n)
{
	CelestialBody *cb = (CelestialBody*)hPlanet;
	return (n < cb->nJcoeff() ? cb->Jcoeff(n) : 0.0);
}

DLLEXPORT void oapiWriteLog (char *line)
{
	LOGOUT ("%s", line);
}

DLLEXPORT void oapiWriteLogV (const char *format, ...)
{
#ifdef GENERATE_LOG
	va_list ap;
	va_start(ap, format);
	LogOutVA(format, ap);
	va_end(ap);
#endif
}

DLLEXPORT void __writeLogError(const char *func, const char *file, int line, const char *format, ...)
{
#ifdef GENERATE_LOG
	va_list ap;
	va_start(ap, format);
	LogOut_ErrorVA(func, file, line, format, ap);
	va_end(ap);
#endif
}

#ifdef ORBITER_NATIVE_CORE
DLLEXPORT int oapiGetObjectType(OBJHANDLE handle)
{
    if(!handle || !g_psys)return OBJTP_INVALID;
    for(DWORD i=0;i<g_psys->nGrav();++i) {
        auto body=g_psys->GetGravObj(i);
        if(body==handle)return body->Type();
        if(auto planet=dynamic_cast<Planet *>(body))
            for(DWORD j=0;j<planet->nBase();++j)if(planet->GetBase(j)==handle)return OBJTP_SURFBASE;
    }
    for(auto vessel:g_psys->GetVessels())if(vessel==handle)return vessel->Type();
    return OBJTP_INVALID;
}
#endif

#ifdef ORBITER_NATIVE_CORE
DLLEXPORT void oapiGlobalToEqu(OBJHANDLE object,const VECTOR3 &global,double *longitude,double *latitude,double *radius)
{
    auto body=static_cast<Body *>(object);
    if(body && body->s0 && longitude && latitude && radius)
        body->GlobalToEquatorial(Vector(global.x,global.y,global.z),*longitude,*latitude,*radius);
}
#endif

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


DLLEXPORT double oapiGetMaxFuelMass (OBJHANDLE hVessel)
{
	return (((Vessel*)hVessel)->ntank ? ((Vessel*)hVessel)->tank[0]->maxmass : 0.0);
}

DLLEXPORT double oapiRand ()
{
	static double irmax = 1.0/(double)RAND_MAX;
	return (double)rand() * irmax;
}
