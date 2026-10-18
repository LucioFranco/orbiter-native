#include "Orbiter.h"
#include "NativeSound.h"
#include "soundlib.h"
#include "soundevents.h"
#include <filesystem>
#include <iostream>
#include <vector>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv){try {
    require(argc==2,"paired runtime required");std::filesystem::current_path(argv[1]);
    orbiter::NativeAudioSession audio(orbiter::platform::AudioMixer::Output::Offline);
    Orbiter host;host.Launch("NasspPair");auto vessel=oapiGetFocusInterface();
    SoundLib library;char category[]="ProjectApollo",mission[]="Apollo11",table[]="sound.csv";
    library.InitSoundLib(vessel,category);library.SetSoundLibMissionPath(mission);
    SoundEvent event;require(event.LoadMissionLandingSoundArray(library,table) && event.isValid(),"load real Apollo 11 callout table");
    char file[255]{};double offset=0;int newbuffer=0;const double met=102*3600+23*60+57;
    require(!event.play(library,vessel,file,&offset,&newbuffer,met,met-1,3,0,0,0),"wait before first callout MET");
    require(event.play(library,vessel,file,&offset,&newbuffer,met,met,3,0,0,0),"select original first callout");
    require(std::string(file).find("ap11landingmono81.wav")!=std::string::npos && newbuffer,"correct recording selected");
    require(event.PlaySound(file,newbuffer,offset) && event.IsPlaying() && event.AlreadyPlayed(),"native callout starts");
    std::vector<float> samples(48000*2);require(audio.Render(samples.data(),48000),"mix mission recording");
    double energy=0;for(auto value:samples)energy+=value*value;
    require(energy>0.001,"original Apollo 11 callout produces audible PCM");
    const auto saved=event.SaveNativeState();
    SoundEvent resumed;require(resumed.LoadMissionLandingSoundArray(library,table),"load resume event table");
    event.Stop();
    require(resumed.RestoreNativeState(saved,vessel) && resumed.IsPlaying() && resumed.AlreadyPlayed(),
            "restore active recording and event progress");
    require(!resumed.RestoreNativeState("1 999999 1 0 0 0 1 \"\"",vessel) && resumed.IsPlaying(),
            "invalid progress does not destroy existing playback");
    require(resumed.Finish(0.1),"restored recording retains its advanced playback cursor");
    require(!resumed.play(library,vessel,file,&offset,&newbuffer,met,met,3,0,0,0),"resume does not replay a consumed event");
    require(event.RestoreNativeState(saved,vessel),"restore original probe for boundary check");
    require(event.Finish(0.1) && !event.IsPlaying(),"offset boundary stops mission recording");
    require(!event.play(library,vessel,file,&offset,&newbuffer,met,met,3,0,0,0),"selected callout does not repeat");
    // The automatic approach entry point gates on powered guidance and descent stage.
    require(!event.UpdateLandingApproach(library,vessel,"63",met,1,true),"braking program does not trigger approach calls");
    require(!event.UpdateLandingApproach(library,vessel,"64",met,2,true),"ascent stage disables approach calls");
    require(!event.UpdateLandingApproach(library,vessel,"64",met,1,false),"unpowered display disables approach calls");
    event.makeInvalid();require(!event.isValid(),"invalidate clears per-instance schedule");
    char eagleName[]="Eagle";auto eagle=oapiGetVesselByName(eagleName);auto lm=oapiGetVesselInterface(eagle);
    require(lm,"LM available for approach trigger");
    host.SetFocusObject(reinterpret_cast<Vessel *>(eagle),false);
    VESSELSTATUS2 low{};low.version=2;lm->GetStatusEx(&low);
    low.rpos=unit(low.rpos)*(oapiGetSize(low.rbody)+100);lm->DefSetStateEx(&low);
    host.StepSimulation(1.0/60.0);
    SoundLib lmLibrary;lmLibrary.InitSoundLib(lm,category);lmLibrary.SetSoundLibMissionPath(mission);
    SoundEvent approach;require(approach.LoadMissionLandingSoundArray(lmLibrary,table),"load LM approach recording table");
    require(approach.UpdateLandingApproach(lmLibrary,lm,"64",met,1,true) && approach.IsPlaying(),
            "powered P64 near the lunar surface starts an original approach recording");
    require(!approach.UpdateLandingApproach(lmLibrary,lm,"00",met,1,true) && !approach.IsPlaying(),
            "leaving landing guidance stops approach recording");
    std::cout<<"Original Apollo 11 callout selection, playback and offset stop verified\n";
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
