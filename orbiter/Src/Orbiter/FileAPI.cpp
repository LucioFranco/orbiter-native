// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef OAPI_IMPLEMENTATION
#define OAPI_IMPLEMENTATION
#endif

#include "Orbiter.h"
#ifdef ORBITER_NATIVE_CORE
#include "NativeMFD.h"
#endif
#include "Config.h"
#include "State.h"
#include "Psys.h"
#include "Camera.h"
#include "Pane.h"
#include "Vessel.h"
#include "Platform/DynamicLibrary.h"
#include "Util.h"
#include "Log.h"
#include "Platform/Vfs.h"

using namespace std;
extern Orbiter *g_pOrbiter;

DLLEXPORT FILEHANDLE oapiOpenFile (const char *fname, FileAccessMode mode, PathRoot root)
{
	if (!fname) return nullptr;
	std::string path;
	switch (root) {
	case CONFIG:
		path = g_pOrbiter->Cfg()->ConfigPathNoext(fname);
		break;
	case SCENARIOS:
		path = g_pOrbiter->ScnPath(fname);
		break;
	case TEXTURES:
		path = g_pOrbiter->TexPath(fname);
		break;
	case TEXTURES2:
		path = g_pOrbiter->HTexPath(fname);
		break;
	case MESHES:
		path = g_pOrbiter->MeshPath(fname);
		break;
	default:
		path = fname;
		break;
	}

	const auto cbuf = orbiter::platform::ResolvePath(path);
	switch (mode) {
	case FILE_IN:
		return (FILEHANDLE)(new ifstream (cbuf));
	case FILE_IN_ZEROONFAIL: {
		ifstream *ifs = new ifstream (cbuf);
		if (ifs->fail()) {
			delete ifs;
			ifs = 0;
		}
		return (FILEHANDLE)ifs;
		}
	case FILE_OUT:
		TRACENEW; return (FILEHANDLE)(new ofstream (cbuf));
	case FILE_APP:
		TRACENEW; return (FILEHANDLE)(new ofstream (cbuf, ios::app));
	}
	return 0;
}

DLLEXPORT void oapiCloseFile (FILEHANDLE file, FileAccessMode mode)
{
	if (file) {
		switch (mode) {
		case FILE_IN:
		case FILE_IN_ZEROONFAIL:
			delete (ifstream*)file;
			break;
		case FILE_OUT:
		case FILE_APP:
			delete (ofstream*)file;
			break;
		}
	}
}

DLLEXPORT void oapiWriteLine (FILEHANDLE file, char *line)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << line << endl;
}

// Native modules use the same animation-state scenario format as OrbiterAPI.cpp.
#ifdef ORBITER_NATIVE_CORE
DLLEXPORT void WriteScenario_state(FILEHANDLE file, char *tag, const AnimState &state)
{
    char buffer[256];
    std::snprintf(buffer,sizeof(buffer),"%d %0.4f",state.action-1,state.pos);
    oapiWriteScenario_string(file,tag,buffer);
}
DLLEXPORT void sscan_state(char *text, AnimState &state)
{
    int action; double position;
    if(std::sscanf(text,"%d%lf",&action,&position)==2) {
        state.action=static_cast<AnimState::Action>(action+1);state.pos=position;
    }
}
#endif

DLLEXPORT void oapiWriteScenario_string (FILEHANDLE file, char *item, char *string)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << "  " << item << ' ' << string << endl;
}

DLLEXPORT void oapiWriteScenario_int (FILEHANDLE file, char *item, int i)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << "  " << item << ' ' << i << endl;
}

DLLEXPORT void oapiWriteScenario_float (FILEHANDLE file, char *item, double d)
{
	ofstream &ofs = *(ofstream*)file;
	FltFormat f{ 6 }; // default precision: 6
	ofs << "  " << item << ' ' << f(d) << endl;
}

DLLEXPORT void oapiWriteScenario_vec (FILEHANDLE file, char *item, const VECTOR3 &vec)
{
	ofstream &ofs = *(ofstream*)file;
	FltFormat f{ 6 }; // default precision: 6
	ofs << "  " << item << ' ' << f(vec.x) << ' ' << f(vec.y) << ' ' << f(vec.z) << endl;
}

DLLEXPORT bool oapiReadScenario_nextline (FILEHANDLE file, char *&line)
{
	ifstream &ifs = *(ifstream*)file;
	char *cbuf = readline(ifs);
	if (!cbuf) return false;
	line = trim_string (cbuf);
	if (!_stricmp (line, "END")) return false;
	return true;
}

DLLEXPORT void oapiWriteItem_string (FILEHANDLE file, char *item, char *string)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << item << " = " << string << endl;
}

DLLEXPORT void oapiWriteItem_float (FILEHANDLE file, char *item, double d)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << item << " = " << d << endl;
}

DLLEXPORT void oapiWriteItem_int (FILEHANDLE file, char *item, int i)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << item << " = " << i << endl;
}

DLLEXPORT void oapiWriteItem_bool (FILEHANDLE file, char *item, bool b)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << item << " = " << (b ? "TRUE":"FALSE") << endl;
}

DLLEXPORT void oapiWriteItem_vec (FILEHANDLE file, char *item, const VECTOR3 &vec)
{
	ofstream &ofs = *(ofstream*)file;
	ofs << item << " = " << vec.x << ' ' << vec.y << ' ' << vec.z << endl;
}

DLLEXPORT bool oapiReadItem_string (FILEHANDLE f, char *item, char *string)
{
	return GetItemString (*(ifstream*)f, item, string);
}

DLLEXPORT bool oapiReadItem_float (FILEHANDLE f, char *item, double &val)
{
	return GetItemReal (*(ifstream*)f, item, val);
}

DLLEXPORT bool oapiReadItem_int (FILEHANDLE f, char *item, int &val)
{
	return GetItemInt (*(ifstream*)f, item, val);
}

DLLEXPORT bool oapiReadItem_bool (FILEHANDLE f, char *item, bool &val)
{
	return GetItemBool (*(ifstream*)f, item, val);
}

DLLEXPORT bool oapiReadItem_vec (FILEHANDLE f, char *item, VECTOR3 &val)
{
	Vector vec;
	bool res = GetItemVector (*(ifstream*)f, item, vec);
	val.x = vec.x;
	val.y = vec.y;
	val.z = vec.z;
	return res;
}

extern PlanetarySystem *g_psys;
extern Camera *g_camera;
extern Pane *g_pane;
extern Vessel *g_focusobj;

bool Orbiter::SaveScenario (const char *fname, const char *desc, int desc_type)
{
	if (!pState || !g_psys || !g_camera || !g_focusobj || !fname) return false;
	pState->Update (g_focusobj->Name());

	ofstream ofs (ScnPath (fname));
	if (ofs) {
		// save scenario state
		pState->Write(ofs, desc, desc_type, 0);
		//pState->Write(ofs, 0, pConfig->CfgDebugPrm.bSaveExitScreen ? "CurrentState_img" : "CurrentState");
		g_camera->Write (ofs);
		if (g_pane) g_pane->Write (ofs);
#ifdef ORBITER_NATIVE_CORE
        orbiter::NativeMFDWrite(ofs);
#endif
		g_psys->Write (ofs);

		// let plugins save their states to the scenario file
		for (auto it = m_Plugin.begin(); it != m_Plugin.end(); it++) {
			void (*opcSaveState)(FILEHANDLE) = (void(*)(FILEHANDLE))orbiter::platform::LibrarySymbol(it->hDLL, "opcSaveState");
			if (opcSaveState) {
				ofs << std::endl << "BEGIN_" << it->sName << std::endl;
				opcSaveState((FILEHANDLE)&ofs);
				ofs << "END" << std::endl;
			}
		}
		ofs.close();
		return !ofs.fail();
	} else
		return false;
}

DLLEXPORT bool oapiSaveScenario (const char *fname, const char *desc)
{
	return g_pOrbiter->SaveScenario (fname, desc, 1);
}
