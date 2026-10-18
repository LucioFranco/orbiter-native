// Copyright (c) Martin Schweiger
// Licensed under the MIT License
#include "Orbiter.h"
#include "Mesh.h"
#include "GraphicsAPI.h"
#include "Log.h"
#include <fstream>
using namespace std;
extern Orbiter *g_pOrbiter;

const Mesh *Orbiter::LoadMeshGlobal (const char *fname)
{
	const Mesh *mesh =  meshmanager.LoadMesh (fname);
	if (mesh && gclient) gclient->clbkStoreMeshPersistent ((MESHHANDLE)mesh, fname);
	return mesh;
}

const Mesh *Orbiter::LoadMeshGlobal (const char *fname, LoadMeshClbkFunc fClbk)
{
	bool firstload = false;
	const Mesh *mesh =  meshmanager.LoadMesh (fname, &firstload);
	if (mesh && fClbk) fClbk ((MESHHANDLE)mesh, firstload);
	if (mesh && gclient) gclient->clbkStoreMeshPersistent ((MESHHANDLE)mesh, fname);
	return mesh;
}

DLLEXPORT MESHHANDLE oapiLoadMesh (const char *fname)
{
	ifstream ifs(g_pOrbiter->MeshPath(fname));
	Mesh *mesh = new Mesh; TRACENEW
	ifs >> *mesh;
	return (MESHHANDLE)mesh;
}

DLLEXPORT const MESHHANDLE oapiLoadMeshGlobal (const char *fname)
{
	return (const MESHHANDLE)g_pOrbiter->LoadMeshGlobal (fname);
}

DLLEXPORT const MESHHANDLE oapiLoadMeshGlobal (const char *fname, LoadMeshClbkFunc fClbk)
{
	return (const MESHHANDLE)g_pOrbiter->LoadMeshGlobal (fname, fClbk);
}

DLLEXPORT const char* oapiGetMeshFilename(MESHHANDLE hMesh)
{
	return hMesh ? ((Mesh*)hMesh)->GetName() : NULL;
}

DLLEXPORT MESHHANDLE oapiCreateMesh (DWORD ngrp, MESHGROUP *grp)
{
	Mesh *mesh = new Mesh;
	for (DWORD i = 0; i < ngrp; i++) {
		mesh->AddGroup (grp[i].Vtx, grp[i].nVtx, grp[i].Idx, grp[i].nIdx, grp[i].MtrlIdx, grp[i].TexIdx, grp[i].zBias, 0, true);
	}
	return (MESHHANDLE)mesh;
}

DLLEXPORT void oapiDeleteMesh (MESHHANDLE hMesh)
{
	Mesh *mesh = (Mesh*)hMesh;
	if (mesh) delete mesh;
}

DLLEXPORT SURFHANDLE oapiGetTextureHandle (MESHHANDLE hMesh, DWORD texidx)
{
	Mesh *mesh = (Mesh*)hMesh;
	return (mesh ? mesh->GetTexture (texidx-1) : 0);
}

DLLEXPORT bool oapiSetTexture (MESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex)
{
	Mesh *mesh = (Mesh*)hMesh;
	return mesh->SetTexture (texidx-1, tex);
}

DLLEXPORT bool oapiSetTexture (DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkSetMeshTexture (hMesh, texidx, tex) : false);
}

DLLEXPORT DWORD oapiMeshGroupCount (MESHHANDLE hMesh)
{
	if (!hMesh) {
		static bool errecho = true;
		if (errecho) {
			LOGOUT_ERR("Invalid mesh handle. Returned 0. (Continuing)");
			errecho = false;
		}
		return 0;
	}
	return ((Mesh*)hMesh)->nGroup();
}

DLLEXPORT DWORD oapiGetMeshFlags (MESHHANDLE hMesh)
{
	if (!hMesh) {
		static bool errecho = true;
		if (errecho) {
			LOGOUT_ERR("Invalid mesh handle. Returned NULL. (Continuing)");
			errecho = false;
		}
		return 0;
	}
	return ((Mesh*)hMesh)->GetFlags();
}

DLLEXPORT MESHGROUP *oapiMeshGroup (MESHHANDLE hMesh, DWORD idx)
{
	if (!hMesh) {
		static bool errecho = true;
		if (errecho) {
			LOGOUT_ERR("Invalid mesh handle. Returned NULL. (Continuing)");
			errecho = false;
		}
		return 0;
	}
	return (MESHGROUP*)(((Mesh*)hMesh)->GetGroup (idx));
}

DLLEXPORT MESHGROUP *oapiMeshGroup (DEVMESHHANDLE hMesh, DWORD idx)
{
	// Device-specific meshes don't allow direct access to group data
	return NULL;
}

DLLEXPORT MESHGROUPEX *oapiMeshGroupEx (MESHHANDLE hMesh, DWORD idx)
{
	return (MESHGROUPEX*)(((Mesh*)hMesh)->GetGroup (idx));
}

DLLEXPORT DWORD oapiAddMeshGroup (MESHHANDLE hMesh, MESHGROUP *grp)
{
	return (DWORD)((Mesh*)hMesh)->AddGroup (grp->Vtx, grp->nVtx, grp->Idx, grp->nIdx,
		grp->MtrlIdx, grp->TexIdx, grp->zBias, 0, true);
}

DLLEXPORT bool oapiAddMeshGroupBlock (MESHHANDLE hMesh, DWORD grpidx,
	const NTVERTEX *vtx, DWORD nvtx, const WORD *idx, DWORD nidx)
{
	return ((Mesh*)hMesh)->AddGroupBlock (grpidx, vtx, nvtx, idx, nidx);
}

DLLEXPORT DWORD oapiMeshMaterialCount (MESHHANDLE hMesh)
{
	return ((Mesh*)hMesh)->nMaterial();
}

DLLEXPORT MATERIAL *oapiMeshMaterial (MESHHANDLE hMesh, DWORD idx)
{
	return (MATERIAL*)(((Mesh*)hMesh)->GetMaterial (idx));
}

DLLEXPORT int oapiMeshMaterial (DEVMESHHANDLE hMesh, DWORD matidx, MATERIAL *mat)
{
	oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
	return (gc ? gc->clbkMeshMaterial (hMesh, matidx, mat) : 1);
}

DLLEXPORT int oapiSetMaterial(DEVMESHHANDLE mesh, DWORD index, const MATERIAL *material)
{
    auto gc=g_pOrbiter->GetGraphicsClient();
    return gc?gc->clbkSetMeshMaterial(mesh,index,material):1;
}
