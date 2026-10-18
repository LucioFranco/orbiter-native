/* Modified for native portability. Publication notice added 2026-10-18.
 * Original copyright, license grants and linking exceptions remain unchanged.
 * Behavioral changes are inventoried in upstream/nassp-v9/manifest.json.
 */
#include "Orbitersdk.h"
#include "soundlib.h"
#include "soundevents.h"
#include "Platform/Vfs.h"
#include <fstream>
#include "NativeSound.h"
#include <cstring>
#include <iomanip>
#include <memory>
// Playback scheduling is being migrated from the legacy DirectSound path.
SoundEvent::SoundEvent() : altitude(-1), mode(-1), met(0), played(false),
    filenames{}, offset(0), timetoignition(0), timeafterignition(0),
    timetoapproach(0), mandatory(0) {}
SoundEvent::~SoundEvent() { delete nativeAudio; }
int SoundEvent::isValid() { return !nativeEvents.empty(); }
int SoundEvent::makeInvalid() { Stop();nativeEvents.clear();nativeNext=0;played=false;return 1; }
bool SoundEvent::AlreadyPlayed() { return played; }
int SoundEvent::play(const SoundLib &, VESSEL *vessel, char *names, double *start, int *newbuffer,
    double computerTime, double missionTime, int phase, double remaining, double sinceIgnition,
    double approach, int flags, int volume) {
    if(!vessel || !names || !start || !newbuffer || !isValid())return 0;
    if(!nativeAudio)nativeAudio=XRSound::CreateInstance(vessel);
    if(!nativeAudio->IsPresent())return 0;
    if(IsPlaying()) {
        if(nativeNext>=nativeEvents.size() || nativeEvents[nativeNext].file!=nativeFile ||
           nativeEvents[nativeNext].offset<0 || !Finish(nativeEvents[nativeNext].offset-0.1))return 0;
    }
    const double elapsed=missionTime-computerTime;
    const nassp::native::LandingClock clock{phase,missionTime,remaining-elapsed,
        sinceIgnition+elapsed,approach-elapsed,vessel->GetAltitude(ALTMODE_GROUND)-vessel->GetCOG_elev()};
    const auto selected=nassp::native::SelectLandingSound(nativeEvents,nativeNext,clock);
    if(!selected)return 0;
    const auto &event=nativeEvents[selected->index];
    if(event.file.size()>=255)return 0;
    std::strcpy(names,event.file.c_str());*start=selected->offset;*newbuffer=event.file!=nativeFile;
    nativeLoop=flags==LOOP;nativeVolume=std::clamp(volume/255.0f,0.0f,1.0f);
    nativeNext=selected->index+1;return 1;
}
int SoundEvent::Stop() { return nativeAudio && nativeAudio->StopWav(1); }
int SoundEvent::Done() { Stop();return 1; }
int SoundEvent::LoadMissionLandingSoundArray(const SoundLib &library, char *name) {
    if(!name)return 0;
    try {
        const std::string directory=std::string(library.basepath)+"/"+library.missionpath;
        std::ifstream input(orbiter::platform::ResolvePath(directory+"/"+name));
        auto events=nassp::native::ReadLandingSounds(input,directory);
        Stop();nativeEvents=std::move(events);nativeNext=0;played=false;return 1;
    }catch(const std::exception &) {return 0;}
}
int SoundEvent::LoadMissionTimeSoundArray(const SoundLib &, char *, double) { return 0; }
int SoundEvent::PlaySound(char *name, int newbuffer, double start) {
    if(!nativeAudio || !name || !std::isfinite(start))return 0;
    if(newbuffer || nativeFile!=name) {
        if(!nativeAudio->LoadWav(1,name,XRSound::PlaybackType::Radio))return 0;
        nativeFile=name;
    }
    if(!nativeAudio->PlayWavFrom(1,std::max(0.0,start),nativeLoop,nativeVolume))return 0;
    played=true;return 1;
}
int SoundEvent::IsPlaying() { return nativeAudio && nativeAudio->IsWavPlaying(1); }
int SoundEvent::Finish(double finish) {
    if(!nativeAudio || !std::isfinite(finish) || finish<0)return 0;
    if(nativeAudio->GetWavPosition(1)<=finish)return 0;
    return Stop();
}

bool SoundEvent::UpdateLandingApproach(const SoundLib &library, VESSEL *vessel,
                                      const char *program, double missionTime, int stage, bool powered) {
    if(!powered || stage>=2 || !program || (std::strcmp(program,"64") &&
       std::strcmp(program,"65") && std::strcmp(program,"66"))) {Stop();return false;}
    char name[255]{};double start=0;int fresh=0;
    if(!play(library,vessel,name,&start,&fresh,missionTime,missionTime,2,0,0,0))return false;
    return PlaySound(name,fresh,start)!=0;
}

std::string SoundEvent::SaveNativeState() const {
    const bool active=nativeAudio && nativeAudio->IsWavPlaying(1);
    std::ostringstream out;
    out<<std::setprecision(17)<<1<<' '<<nativeNext<<' '<<played<<' '<<active<<' '
       <<(active?nativeAudio->GetWavPosition(1):0)<<' '<<nativeLoop<<' '<<nativeVolume<<' '
       <<std::quoted(active?nativeFile:std::string{});
    return out.str();
}
bool SoundEvent::RestoreNativeState(const std::string &state,VESSEL *vessel) {
    std::istringstream in(state);int version=0;size_t next=0;bool wasPlayed=false,active=false,loop=false;
    double position=0;float volume=0;std::string file;
    if(!(in>>version>>next>>wasPlayed>>active>>position>>loop>>volume>>std::quoted(file)) ||
       (in>>std::ws,!in.eof()) || version!=1 || next>nativeEvents.size() ||
       !std::isfinite(position) || position<0 || !std::isfinite(volume) || volume<0 || volume>1)return false;
    if(active && (!vessel || !next || file!=nativeEvents[next-1].file))return false;
    std::unique_ptr<XRSound> bank;
    if(active) {
        bank.reset(XRSound::CreateInstance(vessel));
        if(bank->IsPresent() && (!bank->LoadWav(1,file.c_str(),XRSound::PlaybackType::Radio) ||
            !bank->PlayWavFrom(1,position,loop,volume)))return false;
    }
    delete nativeAudio;nativeAudio=bank.release();nativeNext=next;played=wasPlayed;
    nativeFile=active?file:std::string{};nativeLoop=loop;nativeVolume=volume;return true;
}
