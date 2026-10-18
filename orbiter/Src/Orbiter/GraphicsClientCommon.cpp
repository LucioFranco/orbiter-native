// Shared device-independent GraphicsClient defaults.
#include "GraphicsAPI.h"
namespace oapi {
int GraphicsClient::clbkVisEvent (OBJHANDLE hObj, VISHANDLE vis, DWORD msg, DWORD_PTR context)
{
	return 0;
}

ParticleStream *GraphicsClient::clbkCreateParticleStream (PARTICLESTREAMSPEC *pss)
{
	return NULL;
}

ParticleStream *GraphicsClient::clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel, const double *lvl, const VECTOR3 &ref, const VECTOR3 &dir)
{
	return NULL;
}

ParticleStream *GraphicsClient::clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir)
{
	return NULL;
}

ParticleStream *GraphicsClient::clbkCreateReentryStream (PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel)
{
	return NULL;
}

void GraphicsClient::clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, float alpha, bool additive)
{
	// if not implemented by the client, just use default (opaque) rendering
	clbkRender2DPanel (hSurf, hMesh, T, additive);
}

void GraphicsClient::clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, bool additive)
{
	// can we do any default device-independent processing here?
}

int GraphicsClient::clbkBeginBltGroup (SURFHANDLE tgt)
{
	if (tgt == RENDERTGT_NONE)
		return -3;
	if (surfBltTgt != RENDERTGT_NONE && surfBltTgt != tgt)
		return -2;
	surfBltTgt = tgt;
	return 0;
}

int GraphicsClient::clbkEndBltGroup ()
{
	if (surfBltTgt == RENDERTGT_NONE)
		return -2;
	surfBltTgt = RENDERTGT_NONE;
	return 0;
}
}
