// Copyright (c) Martin Schweiger
// Licensed under the MIT License
// Shared default flight controls; platform adapters supply the DIK key state.
#include "Orbiter.h"
#include "Vessel.h"
#include "Pane.h"
extern Vessel *g_focusobj;
extern Pane *g_pane;
extern TimeData td;

void Orbiter::KbdInputImmediate_OnRunning (char *kstate)
{
	if (g_focusobj->ConsumeDirectKey (kstate)) return;  // key is consumed by focus vessel

	// main/retro/hover thruster settings
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_IncMainThrust))   g_focusobj->IncMainRetroLevel ( 0.2*td.SimDT);
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_DecMainThrust))   g_focusobj->IncMainRetroLevel (-0.2*td.SimDT);
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_KillMainRetro)) { g_focusobj->SetThrusterGroupLevel (THGROUP_MAIN, 0.0);
		                                                         g_focusobj->SetThrusterGroupLevel (THGROUP_RETRO, 0.0); }
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_FullMainThrust))  g_focusobj->OverrideMainLevel ( 1.0);
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_FullRetroThrust)) g_focusobj->OverrideMainLevel (-1.0);
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_IncHoverThrust))  g_focusobj->IncThrusterGroupLevel (THGROUP_HOVER,  0.2*td.SimDT);
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_DecHoverThrust))  g_focusobj->IncThrusterGroupLevel (THGROUP_HOVER, -0.2*td.SimDT);

	// Reaction control system
	if (bEnableAtt) {
		// rotational mode
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSPitchUp))     ctrlKeyboard[THGROUP_ATT_PITCHUP]   = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSPitchUp))   ctrlKeyboard[THGROUP_ATT_PITCHUP]   =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSPitchDown))   ctrlKeyboard[THGROUP_ATT_PITCHDOWN] = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSPitchDown)) ctrlKeyboard[THGROUP_ATT_PITCHDOWN] =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSYawLeft))     ctrlKeyboard[THGROUP_ATT_YAWLEFT]   = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSYawLeft))   ctrlKeyboard[THGROUP_ATT_YAWLEFT]   =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSYawRight))    ctrlKeyboard[THGROUP_ATT_YAWRIGHT]  = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSYawRight))  ctrlKeyboard[THGROUP_ATT_YAWRIGHT]  =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSBankLeft))    ctrlKeyboard[THGROUP_ATT_BANKLEFT]  = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSBankLeft))  ctrlKeyboard[THGROUP_ATT_BANKLEFT]  =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSBankRight))   ctrlKeyboard[THGROUP_ATT_BANKRIGHT] = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSBankRight)) ctrlKeyboard[THGROUP_ATT_BANKRIGHT] =  100;
		// linear mode
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSUp))          ctrlKeyboard[THGROUP_ATT_UP]        = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSUp))        ctrlKeyboard[THGROUP_ATT_UP]        =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSDown))        ctrlKeyboard[THGROUP_ATT_DOWN]      = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSDown))      ctrlKeyboard[THGROUP_ATT_DOWN]      =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSLeft))        ctrlKeyboard[THGROUP_ATT_LEFT]      = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSLeft))      ctrlKeyboard[THGROUP_ATT_LEFT]      =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSRight))       ctrlKeyboard[THGROUP_ATT_RIGHT]     = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSRight))     ctrlKeyboard[THGROUP_ATT_RIGHT]     =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSForward))     ctrlKeyboard[THGROUP_ATT_FORWARD]   = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSForward))   ctrlKeyboard[THGROUP_ATT_FORWARD]   =  100;
		if      (keymap.IsLogicalKey (kstate, OAPI_LKEY_RCSBack))        ctrlKeyboard[THGROUP_ATT_BACK]      = 1000;
		else if (keymap.IsLogicalKey (kstate, OAPI_LKEY_LPRCSBack))      ctrlKeyboard[THGROUP_ATT_BACK]      =  100;
	}

	// Elevator trim control
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_IncElevatorTrim)) g_focusobj->IncTrim (AIRCTRL_ELEVATORTRIM);
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_DecElevatorTrim)) g_focusobj->DecTrim (AIRCTRL_ELEVATORTRIM);

	// Wheel brake control
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_WheelbrakeLeft))  g_focusobj->SetWBrakeLevel (1.0, 1, false);
	if (keymap.IsLogicalKey (kstate, OAPI_LKEY_WheelbrakeRight)) g_focusobj->SetWBrakeLevel (1.0, 2, false);

	// The native host has no MFD manager yet.
#ifndef ORBITER_NATIVE_CORE
	// left/right MFD control
	if (g_pane && KEYMOD_SHIFT (kstate)) {
		if (KEYMOD_LSHIFT (kstate) && g_pane->MFD(0)) g_pane->MFD(0)->ConsumeKeyImmediate (kstate);
		if (KEYMOD_RSHIFT (kstate) && g_pane->MFD(1)) g_pane->MFD(1)->ConsumeKeyImmediate (kstate);
	}
#endif
}

