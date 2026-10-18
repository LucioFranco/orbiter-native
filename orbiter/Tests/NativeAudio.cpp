#include "Platform/Audio.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using orbiter::platform::AudioMixer;
static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static double level(const std::vector<float> &samples) {
    double sum = 0; for (float sample : samples) { require(std::isfinite(sample), "finite mixed PCM"); sum += std::abs(sample); }
    return sum / samples.size();
}
int main(int argc, char **argv) {
    try {
        require(argc == 3, "fixture and NASSP audio paths required");
        const auto root = std::filesystem::path(argv[1]); std::filesystem::create_directories(root);
        const auto path = root / "ToNe.WAV";
        {
            std::ofstream out(path, std::ios::binary);
            auto word = [&](unsigned value, int bytes) { for (int i = 0; i < bytes; ++i) out.put(char(value >> (8*i))); };
            out.write("RIFF",4); word(36+1920,4); out.write("WAVEfmt ",8); word(16,4);
            word(1,2); word(1,2); word(48000,4); word(96000,4); word(2,2); word(16,2);
            out.write("data",4); word(1920,4); for (int i=0;i<960;++i) word(8192,2);
        }
        const auto offsetPath=root/"offset.wav";
        {
            std::ofstream out(offsetPath,std::ios::binary);
            auto word=[&](unsigned value,int bytes){for(int i=0;i<bytes;++i)out.put(char(value>>(8*i)));};
            out.write("RIFF",4);word(36+1600,4);out.write("WAVEfmt ",8);word(16,4);
            word(1,2);word(1,2);word(8000,4);word(16000,4);word(2,2);word(16,2);
            out.write("data",4);word(1600,4);
            for(int i=0;i<800;++i)word(i<400?8192:unsigned(uint16_t(-8192)),2);
        }
        AudioMixer mixer(AudioMixer::Output::Offline);
        auto seeking=mixer.Load(offsetPath.string());
        require(seeking && seeking->PlayFrom(0.075,false,1),"start within an 8 kHz source recording");
        require(std::abs(seeking->Position()-0.075)<1.0/8000,"source-time cursor begins at requested offset");
        std::vector<float> seekSamples(480*2);
        require(mixer.Render(seekSamples.data(),480),"render seeked source");
        require(std::all_of(seekSamples.begin()+32,seekSamples.end(),[](float value){return value<-0.2f;}),
                "seek selects the negative section using source rate, not mixer rate");
        require(seeking->Position()>0.08 && seeking->Position()<0.1,"source cursor advances during resampling");
        require(!seeking->PlayFrom(-1,false,1) && !seeking->PlayFrom(INFINITY,false,1) &&
                !seeking->PlayFrom(0.2,false,1),"invalid offsets rejected");
        seeking->Stop();seeking.reset();
        require(mixer.Render(seekSamples.data(),480),"drain seek fixture tail");
        std::string windowsPath = (root / "tone.wav").string(); std::replace(windowsPath.begin(), windowsPath.end(), '/', '\\');
        auto clip = mixer.Load(windowsPath); require(bool(clip), "load with case and slash insensitive VFS");
        require(!mixer.Load((root / "absent.wav").string()), "missing sound does not report success");
        { std::ofstream out(root / "bad.wav"); out << "not a WAV"; }
        require(!mixer.Load((root / "bad.wav").string()), "bad sound rejected");
        std::vector<float> half(480*2), full(1200*2), paused(2400*2);
        require(!clip->IsPlaying() && clip->Play(false,1), "start one-shot");
        require(mixer.Render(half.data(),480) && level(half)>0.2, "decoded mono WAV mixed to stereo");
        require(clip->Play(false,1), "update existing playback");
        require(mixer.Render(full.data(),1200) && !clip->IsPlaying(), "repeated play did not loop or restart one-shot");
        require(level(full)>0.05 && level(full)<0.13, "remaining one-shot frames followed by silence");
        require(clip->Play(false,1), "restart completed sound");
        require(mixer.Render(half.data(),480), "render first half before pause");
        mixer.SetPaused(true);
        require(mixer.Render(paused.data(),2400) && level(paused)<0.00001, "paused mixer emits silence");
        mixer.SetPaused(false);
        require(mixer.Render(half.data(),480) && level(half)>0.2, "pause preserves sample cursor");
        require(clip->Stop() && clip->Play(true,0.5f), "start looping sound");
        require(mixer.Render(paused.data(),2400) && clip->IsPlaying(), "loop spans file end");
        const double base = level(paused); require(base>0.1 && base<0.14, "half-volume loop");
        clip->SetGain(0.5f); mixer.SetVolume(0.5f);
        require(mixer.Render(paused.data(),2400), "render combined volume");
        require(std::abs(level(paused)/base-0.25)<0.03, "clip gain and master volume multiply");
        require(clip->Stop() && !clip->IsPlaying(), "stop clears playback state");
        require(mixer.Render(full.data(),1200), "render stopped mixer");
        // A graph boundary can retain a previously mixed frame. Bound that
        // tail tightly rather than accepting arbitrary audio after stop.
        require(std::all_of(full.begin()+16, full.end(), [](float sample) { return std::abs(sample)<0.00001f; }),
            "stopped source has no tail beyond eight frames");
        require(mixer.Render(full.data(),1200) && level(full)<0.00001, "stop removes audible samples after buffered tail");
        mixer.SetVolume(1); clip->SetGain(1);
        auto other = mixer.Load(windowsPath);
        require(other && clip->Play(true,0.25f) && other->Play(true,0.5f), "two independent voices");
        require(mixer.Render(paused.data(),2400) && std::abs(level(paused)-0.1875)<0.01, "voices mix additively");
        clip->Stop();
        require(mixer.Render(paused.data(),2400) && std::abs(level(paused)-0.125)<0.01, "stopping one voice preserves the other");
        other.reset();
        std::unique_ptr<orbiter::platform::AudioClip> retained;
        {
            AudioMixer temporary(AudioMixer::Output::Offline);
            retained = temporary.Load(windowsPath);
        }
        require(retained && retained->Play(true,1) && retained->Stop(), "clip retains mixer lifetime");
        retained.reset();
        auto actual = mixer.Load((std::filesystem::path(argv[2]) / "LMSuitFan.wav").string());
        require(actual && actual->Play(true,1), "decode original NASSP cockpit asset");
        std::vector<float> second(48000*2);
        require(mixer.Render(second.data(),48000) && level(second)>0.0001, "original NASSP sound produces nonzero PCM");
        actual.reset();
        require(mixer.Render(full.data(),1200) && level(full)<0.00001, "destroying active clip detaches it from mixer");
        require(!mixer.Render(nullptr,1), "reject null output");
        std::cout << "Native miniaudio decoding, mixing, lifecycle, loop, volume and pause verified\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
