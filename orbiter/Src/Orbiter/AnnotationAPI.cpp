// Copyright (c) Martin Schweiger
// Licensed under the MIT License
// Original annotation implementation shared by the native host and graphics client.
#include "Orbiter.h"
#include "GraphicsAPI.h"
#include <algorithm>
#include <cstring>
extern Orbiter *g_pOrbiter;
namespace oapi {
ScreenAnnotation::ScreenAnnotation (GraphicsClient *_gc)
{
	gc = _gc;
	_gc->clbkGetViewportSize (&viewW, &viewH);
	txt = 0;
	buflen = 0;
	txtscale = 0;
	font = NULL;
	//hFont = NULL;
	Reset();
}

ScreenAnnotation::~ScreenAnnotation ()
{
	if (txt) delete[]txt;
	if (font) gc->clbkReleaseFont (font);
	//DeleteObject (hFont);
}

void ScreenAnnotation::Reset ()
{
	ClearText();
	SetPosition (0.1, 0.1, 0.5, 0.6);
	SetColour (_V(1.0,0.7,0.2));
	SetSize (1.0);
}

void ScreenAnnotation::SetPosition (double x1, double y1, double x2, double y2)
{
	nx1 = (int)(x1*viewW); nx2 = (int)(x2*viewW); nw = nx2-nx1;
	ny1 = (int)(y1*viewH); ny2 = (int)(y2*viewH); nh = ny2-ny1;
}

void ScreenAnnotation::SetText (char *str)
{
	int len = strlen (str)+1;
	if (len > buflen) {
		if (txt) delete []txt;
		txt = new char[len];
		buflen = len;
	}
	strcpy (txt, str);
	txtlen = len-1;
	for (int i = 0; i < txtlen-1; i++)
		if (txt[i] == ' ' && txt[i+1] == ' ') {
			txt[i] = '\r';
			txt[i+1] = '\n';
		}
}

void ScreenAnnotation::ClearText ()
{
	txtlen = 0;
}

void ScreenAnnotation::SetSize (double scale)
{
	if (scale != txtscale) {
		txtscale = scale;
		hf = (int)(viewH*txtscale/35.0);
		if (font) gc->clbkReleaseFont (font);
		font = gc->clbkCreateFont (-hf, true, "Sans");
	}
}

void ScreenAnnotation::SetColour (const VECTOR3 &col)
{
	int r = std::min (255, (int)(col.x*256.0));
	int g = std::min (255, (int)(col.y*256.0));
	int b = std::min (255, (int)(col.z*256.0));
	txtcol  = r | (g << 8) | (b << 16);
	txtcol2 = (r/2) | ((g/2) << 8) | ((b/2) << 16);
}

void ScreenAnnotation::Render ()
{
	if (!txtlen) return;

	Sketchpad *skp = gc->clbkGetSketchpad (0);
	if (skp) {
		skp->SetFont (font);
		skp->SetTextColor (txtcol2);
		skp->SetBackgroundMode (oapi::Sketchpad::BK_TRANSPARENT);
		skp->TextBox (nx1+1, ny1+1, nx2+1, ny2+1, txt, txtlen);
		skp->SetTextColor (txtcol);
		skp->TextBox (nx1, ny1, nx2, ny2, txt, txtlen);
		gc->clbkReleaseSketchpad (skp);
	}
}

}
oapi::ScreenAnnotation *Orbiter::CreateAnnotation (bool exclusive, double size, COLORREF col)
{
	if (!gclient) return NULL;
	oapi::ScreenAnnotation *sn = gclient->clbkCreateAnnotation();
	if (!sn) return NULL;

	sn->SetSize (size);
	VECTOR3 c = { (col      & 0xFF)/256.0,
		         ((col>>8 ) & 0xFF)/256.0,
				 ((col>>16) & 0xFF)/256.0};
	sn->SetColour (c);
	oapi::ScreenAnnotation **tmp = new oapi::ScreenAnnotation*[nsnote+1];
	if (nsnote) {
		memcpy (tmp, snote, nsnote*sizeof(oapi::ScreenAnnotation*));
		delete []snote;
	}
	snote = tmp;
	snote[nsnote++] = sn;
	return sn;

}

bool Orbiter::DeleteAnnotation (oapi::ScreenAnnotation *sn)
{
	DWORD i, j, k;

	if (!gclient) return false;
	for (i = 0; i < nsnote; i++) {
		if (snote[i] == sn) {
			oapi::ScreenAnnotation **tmp = 0;
			if (nsnote > 1) {
				tmp = new oapi::ScreenAnnotation*[nsnote-1];
				for (j = k = 0; j < nsnote; j++)
					if (j != i) tmp[k++] = snote[j];
			}
			delete []snote;
			snote = tmp;
			delete sn;
			nsnote--;
			return true;
		}
	}
	return false;
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
