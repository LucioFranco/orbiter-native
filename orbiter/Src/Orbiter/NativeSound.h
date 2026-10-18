#pragma once
#include "Platform/Audio.h"
#include <memory>
class VESSEL;

namespace orbiter {
// Create before vessels, destroy after them. Absent session retains null audio.
class NativeAudioSession {
public:
    explicit NativeAudioSession(platform::AudioMixer::Output output);
    ~NativeAudioSession();
    NativeAudioSession(const NativeAudioSession &) = delete;
    NativeAudioSession &operator=(const NativeAudioSession &) = delete;
    bool Render(float *samples, std::size_t frames);
    void SetVolume(float volume);
private:
    std::shared_ptr<platform::AudioMixer> mixer_;
};
void UpdateNativeAudio();
void PauseNativeAudio(bool paused);
}

// Native implementation of the XRSound subset used by NASSP. Each instance has
// vessel-local sound IDs; decoding and device output live in the shared core.
class XRSound {
public:
    enum DefaultSound { AudioGreeting, SubsonicCallout, MachCalloutsGroup, OneHundredKnots };
    enum class PlaybackType { InternalOnly, BothViewClose, BothViewMedium, BothViewFar, Radio, Wind, Global };
    static XRSound *CreateInstance(VESSEL *vessel);
    ~XRSound();
    XRSound(const XRSound &) = delete;
    XRSound &operator=(const XRSound &) = delete;
    bool IsPresent() const;
    bool SetDefaultSoundEnabled(DefaultSound, bool enabled);
    bool LoadWav(int id, const char *path, PlaybackType type);
    bool PlayWav(int id, bool loop = false, float volume = 1);
    bool PlayWavFrom(int id, double seconds, bool loop = false, float volume = 1);
    double GetWavPosition(int id) const;
    bool StopWav(int id);
    bool IsWavPlaying(int id) const;
private:
    struct State;
    explicit XRSound(VESSEL *vessel);
    void Update();
    std::unique_ptr<State> state_;
    friend void orbiter::UpdateNativeAudio();
};
