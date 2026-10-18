// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define STRICT 1
#define OAPI_IMPLEMENTATION
#include "CamAPI.h"
#include "Camera.h"
#include "Orbiter.h"
#include <algorithm>

extern Orbiter *g_pOrbiter;

extern Camera *g_camera;

// ======================================================================
// class ExternalCameraControl

ExternalCameraControl::ExternalCameraControl (DWORD dmode, DWORD cmode)
{
	SetDataMode (dmode);
	SetCameraMode (cmode);
	// set default VC parameters
	vcmode.trackrotation = true;
	vcmode.trackposition = true;
	vcmode.freezeonmouse = false;
	vcmode.rotationrange = PI;
	vcmode.positionrange = 0.5;
	vcmode.freeze_t = 1.0;
	// set default track parameters
	trkmode.trackrotation = true;
	trkmode.trackzoom = true;
	trkmode.rotationdata = TrackMode::BYROTATION;
	trkmode.deadzone = 0.2;
	trkmode.speed = 2.0;
}

void ExternalCameraControl::SetDataMode (DWORD mode)
{
	datamode = mode & 0x2F;
}

DWORD ExternalCameraControl::GetDataMode () const
{
	return datamode;
}

void ExternalCameraControl::SetCameraMode (DWORD mode)
{
	cameramode = mode & 0x1F;
}

DWORD ExternalCameraControl::GetCameraMode () const
{
	return cameramode;
}

// ======================================================================
// External control registration

DLLEXPORT bool oapiRegisterExternalCameraControl (ExternalCameraControl *ecc)
{
	return (g_camera ? g_camera->RegisterExternalControl (ecc) : false);
}

DLLEXPORT bool oapiUnregisterExternalCameraControl ()
{
	return (g_camera ? g_camera->UnregisterExternalControl () : false);
}
// Camera functions

DLLEXPORT bool oapiCameraInternal ()
{
	return g_camera->IsInternal();
}

DLLEXPORT OBJHANDLE oapiCameraTarget ()
{
	return (OBJHANDLE)g_camera->Target();
}

DLLEXPORT OBJHANDLE oapiCameraProxyGbody ()
{
	return (OBJHANDLE)g_camera->ProxyPlanet();
}

DLLEXPORT void oapiCameraGlobalPos (VECTOR3 *gpos)
{
	const Vector *gp = g_camera->GPosPtr();
	gpos->x = gp->x;
	gpos->y = gp->y;
	gpos->z = gp->z;
}

DLLEXPORT void oapiCameraGlobalDir (VECTOR3 *gdir)
{
	const Vector gd = g_camera->Direction();
	gdir->x = gd.x;
	gdir->y = gd.y;
	gdir->z = gd.z;
}

DLLEXPORT void oapiCameraRotationMatrix (MATRIX3 *rmat)
{
	const Matrix &m = g_camera->GRot();
	for (int i = 0; i < 9; i++) rmat->data[i] = m.data[i];

}

DLLEXPORT double oapiCameraTargetDist ()
{
	return g_camera->Distance ();
}

DLLEXPORT double oapiCameraAzimuth ()
{
	return g_camera->Phi();
}

DLLEXPORT double oapiCameraPolar ()
{
	return g_camera->Theta();
}

DLLEXPORT double oapiCameraAperture ()
{
	return g_camera->Aperture();
}

DLLEXPORT void oapiCameraSetAperture (double aperture)
{
	g_pOrbiter->SetFOV (std::clamp(aperture, RAD*0.1, RAD*80.0), false);
}

DLLEXPORT void oapiCameraScaleDist (double dscale)
{
	g_camera->ChangeDist (dscale);
}

DLLEXPORT void oapiCameraRotAzimuth (double dazimuth)
{
	g_camera->AddPhi (dazimuth);
}

DLLEXPORT void oapiCameraRotPolar (double dpolar)
{
	g_camera->AddTheta (dpolar);
}

DLLEXPORT void oapiCameraSetCockpitDir (double polar, double azimuth, bool transition)
{
	g_camera->SetCockpitDir (polar, azimuth);
	// this is always set relative to current default camera direction
}

DLLEXPORT void oapiCameraAttach (OBJHANDLE hObj, int mode)
{
	g_pOrbiter->SetView ((Body*)hObj, mode);
}

DLLEXPORT bool oapiSetCameraMode (const CameraMode &mode)
{
	g_camera->SetCMode (&mode);
	return true;
}

DLLEXPORT bool oapiMoveGroundCamera (double forward, double right, double up)
{
	if (g_camera->GetMode() != CAM_GROUNDOBSERVER) return false;
	g_camera->GroundObserverShift(right,forward,up);
	return true;
}

DLLEXPORT int oapiCameraMode ()
{
	return g_camera->GetMode ();
}
