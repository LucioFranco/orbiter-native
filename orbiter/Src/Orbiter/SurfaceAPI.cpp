// Copyright (c) Martin Schweiger
// Licensed under the MIT License
// Shared SDK dispatch to the active graphics client.
#include "Orbiter.h"
#include "Config.h"
#include "GraphicsAPI.h"
#include "DrawAPI.h"
#include "Mesh.h"
#include "Util.h"
#include "Log.h"

extern Orbiter *g_pOrbiter;
extern char DBG_MSG[256];

DLLEXPORT SURFHANDLE oapiLoadTexture (const char *fname, bool dynamic)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) return gc->clbkLoadTexture (fname, 8 & (dynamic ? 3:0));
	else return NULL;
}

DLLEXPORT SURFHANDLE oapiLoadSurfaceEx(const char* fname, DWORD attrib, bool bPath)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	if (gc) return gc->clbkLoadSurface(fname, attrib, bPath);
	else return NULL;
}

DLLEXPORT bool oapiSaveSurface(const char* fname, SURFHANDLE hSrf, oapi::ImageFileFormat fmt, float quality)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	if (gc) return gc->clbkSaveSurfaceToImage(hSrf, fname, fmt, quality);
	return false;
}

DLLEXPORT void oapiReleaseTexture (SURFHANDLE hTex)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkReleaseTexture (hTex);
}

DLLEXPORT int oapiGetMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPREQUESTSPEC *grs)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkGetMeshGroup (hMesh, grpidx, grs) : -1);
}

DLLEXPORT int oapiEditMeshGroup (MESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges)
{
	return ((Mesh*)hMesh)->EditGroup (grpidx, ges);
}

DLLEXPORT int oapiEditMeshGroup (DEVMESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkEditMeshGroup (hMesh, grpidx, ges) : -1);
}

DLLEXPORT int oapiSetMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp mat, const oapi::FVECTOR4* in)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkSetMeshMaterialEx(hMesh, matidx, mat, in) : 1);
}

DLLEXPORT int oapiMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp mat, oapi::FVECTOR4* out)
{
    oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
    return (gc ? gc->clbkMeshMaterialEx(hMesh, matidx, mat, out) : 1);
}

DLLEXPORT oapi::Sketchpad *oapiGetSketchpad (SURFHANDLE surf)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	oapi::Sketchpad *skp = NULL;
	if (gc && surf) skp = gc->clbkGetSketchpad (surf);
	return skp;
}

DLLEXPORT void oapiReleaseSketchpad (oapi::Sketchpad *skp)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc && skp) gc->clbkReleaseSketchpad (skp);
}

DLLEXPORT oapi::Font *oapiCreateFont (int height, bool prop, char *face, FontStyle style)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	oapi::Font *font = NULL;
	if (gc) font = gc->clbkCreateFont (height, prop, face, style);
	return font;
}

DLLEXPORT oapi::Font *oapiCreateFont (int height, bool prop, const char *face, FontStyle style, int orientation)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	oapi::Font *font = NULL;
	if (gc) font = gc->clbkCreateFont (height, prop, face, style, orientation);
	return font;
}

DLLEXPORT oapi::Font* oapiCreateFontEx(int height, char* face, int width, int weight, FontStyle style, float spacing)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	oapi::Font* font = NULL;
	if (gc) font = gc->clbkCreateFontEx(height, face, width, weight, style, spacing);
	return font;
}

DLLEXPORT void oapiReleaseFont (oapi::Font *font)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkReleaseFont (font);
}

DLLEXPORT oapi::Pen *oapiCreatePen (int style, int width, DWORD col)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) return gc->clbkCreatePen (style, width, col);
	else    return NULL;
}

DLLEXPORT void oapiReleasePen (oapi::Pen *pen)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkReleasePen (pen);
}

DLLEXPORT oapi::Brush *oapiCreateBrush (DWORD col)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) return gc->clbkCreateBrush (col);
	else    return NULL;
}

DLLEXPORT void oapiReleaseBrush (oapi::Brush *brush)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkReleaseBrush (brush);
}

DLLEXPORT SURFHANDLE oapiCreateTextureSurface (int width, int height)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkCreateTexture (width, height) : NULL);
}

DLLEXPORT void oapiDestroySurface (SURFHANDLE surf)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkReleaseSurface (surf);
}

DLLEXPORT void oapiClearSurface (SURFHANDLE surf, DWORD col)
{
	if (!surf) return;
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkFillSurface (surf, col);
}

DLLEXPORT void oapiSetSurfaceColourKey (SURFHANDLE surf, DWORD ck)
{
	if (!surf) return;
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkSetSurfaceColourKey (surf, ck);
}

DLLEXPORT DWORD oapiGetColour (DWORD red, DWORD green, DWORD blue)
{
	return GetSurfColour (red, green, blue);
}

DLLEXPORT void oapiBlt (SURFHANDLE tgt, SURFHANDLE src, int tgtx, int tgty, int srcx, int srcy, int w, int h, DWORD ck)
{
	if (!src || !tgt) return;

	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) {
#ifdef ORBITER_NATIVE_CORE
        gc->clbkScaleBltColourKey(tgt,tgtx,tgty,w,h,src,srcx,srcy,w,h,ck);
#else
		if (ck != SURF_NO_CK) {
			static bool bWarnCK = true;
			if (bWarnCK) {
				LogOut_Obsolete (__FUNCTION__, "Colour key argument not supported by graphics client");
				bWarnCK = false;
			}
		}

		dASSERT (gc->clbkBlt (tgt, tgtx, tgty, src, srcx, srcy, w, h), "GraphicsClient::clbkBlt failed");
#endif
	}
}

DLLEXPORT void oapiBlt (SURFHANDLE tgt, SURFHANDLE src, RECT *tgtr, RECT *srcr, DWORD ck, DWORD rotation)
{
	if (!src || !tgt) return;

	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) {
#ifdef ORBITER_NATIVE_CORE
        if(!tgtr || !srcr)return;
        gc->clbkScaleBltColourKey(tgt,tgtr->left,tgtr->top,tgtr->right-tgtr->left,tgtr->bottom-tgtr->top,
            src,srcr->left,srcr->top,srcr->right-srcr->left,srcr->bottom-srcr->top,ck);
#else
		static bool bWarnRot = true;
		if (rotation != SURF_NO_ROTATION) {
			LogOut_Obsolete (__FUNCTION__, "Rotation argument no longer supported");
			bWarnRot = false;
		}

		if (ck != SURF_NO_CK) {
			static bool bWarnCK = true;
			if (bWarnCK) {
				LogOut_Obsolete (__FUNCTION__, "Colour key argument not supported by graphics client");
				bWarnCK = false;
			}
		}
		gc->clbkScaleBlt (tgt, tgtr->left, tgtr->top, tgtr->right-tgtr->left, tgtr->bottom-tgtr->top,
			src, srcr->left, srcr->top, srcr->right-srcr->left, srcr->bottom-srcr->top);
#endif
	}
}

DLLEXPORT int oapiBeginBltGroup (SURFHANDLE tgt)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkBeginBltGroup (tgt) : -1);
}

DLLEXPORT int oapiEndBltGroup ()
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkEndBltGroup () : -1);
}

DLLEXPORT void oapiColourFill (SURFHANDLE tgt, DWORD fillcolor, int tgtx, int tgty, int w, int h)
{
	if (!tgt) return;
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	if (gc) {
		if (w && h)
			gc->clbkFillSurface (tgt, tgtx, tgty, w, h, fillcolor);
		else
			gc->clbkFillSurface (tgt, fillcolor);
	}
}

DLLEXPORT char *oapiDebugString ()
{
	return DBG_MSG;
}

DLLEXPORT void oapiDebugString(const char* str)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	if (gc) gc->clbkDebugString(str);
}

SURFHANDLE Orbiter::RegisterExhaustTexture (char *name)
{
	if (gclient) {
		char path[256];
		strcpy (path, name);
		strcat (path, ".dds");
		return gclient->clbkLoadTexture (path, 0x8);
	} else {
        return NULL;
	}
}

DLLEXPORT void oapiGetViewportSize (DWORD *w, DWORD *h, DWORD *bpp)
{
	*w = g_pOrbiter->ViewW();
	*h = g_pOrbiter->ViewH();
	if (bpp)
		*bpp = g_pOrbiter->ViewBPP();
}

DLLEXPORT double oapiGetPanelScale ()
{
	return g_pOrbiter->Cfg()->CfgLogicPrm.PanelScale;
}

DLLEXPORT SURFHANDLE oapiRegisterExhaustTexture (char *name)
{
	return g_pOrbiter->RegisterExhaustTexture (name);
}

DLLEXPORT SURFHANDLE oapiRegisterReentryTexture (char *name)
{
	return g_pOrbiter->RegisterExhaustTexture (name);
}

DLLEXPORT SURFHANDLE oapiRegisterParticleTexture (char *name)
{
	return g_pOrbiter->RegisterExhaustTexture (name);
}

DLLEXPORT bool oapiUpdateSurface(SURFHANDLE surf, const BYTE *rgba, DWORD pitch)
{
    auto gc = g_pOrbiter->GetGraphicsClient();
    return gc && gc->clbkUpdateSurface(surf, rgba, pitch);
}

DLLEXPORT bool oapiGetSurfaceSize(SURFHANDLE hSrf, int *width, int *height)
{
	oapi::GraphicsClient* gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkGetSurfaceSize(hSrf, (DWORD*)width, (DWORD*)height) : false);
}

DLLEXPORT SURFHANDLE oapiCreateSurface (int width, int height)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkCreateSurface (width, height) : NULL);
}

DLLEXPORT SURFHANDLE oapiCreateSurfaceEx (int width, int height, DWORD attrib)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	SURFHANDLE surf = NULL;
	if (gc) {
		//DWORD attrib = OAPISURFACE_RENDERTARGET | OAPISURFACE_GDI | OAPISURFACE_SKETCHPAD;
		surf = gc->clbkCreateSurfaceEx (width, height, attrib);
	}
	return surf;
}

