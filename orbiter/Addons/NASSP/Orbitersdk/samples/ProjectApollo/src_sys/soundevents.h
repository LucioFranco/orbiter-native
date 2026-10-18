/* Modified for native portability. Publication notice added 2026-10-18.
 * Original copyright, license grants and linking exceptions remain unchanged.
 * Behavioral changes are inventoried in upstream/nassp-v9/manifest.json.
 */
/***************************************************************************
  This file is part of Project Apollo - NASSP
  Copyright 2004-2005



  Project Apollo is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  Project Apollo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Project Apollo; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  See http://nassp.sourceforge.net/license/ for more details.

  **************************************************************************/

#ifndef SOUNDEVENTS_H
#define SOUNDEVENTS_H
#ifdef NASSP_NATIVE
#include "LandingSounds.h"
#endif

//
// Looks like VC6.0 doesn't know about DWORD_PTR.
//

#if defined(_MSC_VER) && _MSC_VER <= 1300
typedef unsigned long *DWORD_PTR;
#endif

// X15 direct sound include to add landing sound

#ifndef NASSP_NATIVE
#include "dsound.h"
#endif

// MODIF x15  managing landing sound

///
/// \ingroup Sound
///
class SoundEvent {

public:
	SoundEvent();
	virtual ~SoundEvent();
	int  isValid();
	int  makeInvalid();

	bool AlreadyPlayed();
	int  play(const SoundLib &soundlib,
		      VESSEL   *vessel,
			  char     *names,
			  double   *offset,
			  int      *newbuffer,
		      double   simcomputert,
			  double   MissionTime,
			  int mode,double timeremaining,double timeafterpdi,double timetoapproach,
		      int flags = NOLOOP, int volume = 255);
#ifdef NASSP_NATIVE
    std::string SaveNativeState() const;
    bool RestoreNativeState(const std::string &state, VESSEL *vessel);
    bool UpdateLandingApproach(const SoundLib &library, VESSEL *vessel,
                               const char *program, double missionTime, int stage, bool powered);
#endif
	int  Stop();
	int  Done();
    int  LoadMissionLandingSoundArray(const SoundLib &soundlib,char *soundname);
    int  LoadMissionTimeSoundArray(const SoundLib &soundlib, char *soundname, double MissionTime);
    int  PlaySound(char *filenames,int newbuffer,double offset);
    int	IsPlaying();
	int	Finish(double offsetfinish);

protected:

	double altitude  ;
	int    mode      ;
	double    met       ;
	bool   played    ;
	char   filenames[255] ;
	int    offset    ;
	double timetoignition;
	double timeafterignition;
	double timetoapproach;
	int    mandatory ;
	
#ifdef NASSP_NATIVE
    std::vector<nassp::native::LandingSound> nativeEvents;
    size_t nativeNext=0;
    XRSound *nativeAudio=nullptr;
    std::string nativeFile;
    bool nativeLoop=false;
    float nativeVolume=1;
#endif
	SoundLib soundlib;
#ifndef NASSP_NATIVE
	LPDIRECTSOUND8  m_pDS;
	LPDIRECTSOUNDBUFFER pDSBPrimary;
    LPDIRECTSOUNDBUFFER* apDSBuffer;

    VOID*   pDSLockedBuffer     ;
    DWORD   dwDSLockedBufferSize;
#endif


};

#endif // SOUNDEVENTS_H
