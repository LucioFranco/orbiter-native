#pragma once
#include <cstddef>
#include <memory>
#include <string_view>
namespace orbiter::platform {
class AudioClip;
// One mixer in liborbiter, shared by all vessel sound banks. Control methods
// run on the simulation thread; miniaudio owns device-thread synchronization.
class AudioMixer {
public:
    enum class Output { Device, Offline };
    explicit AudioMixer(Output output);
    ~AudioMixer();
    AudioMixer(const AudioMixer &) = delete;
    AudioMixer &operator=(const AudioMixer &) = delete;
    std::unique_ptr<AudioClip> Load(std::string_view path);
    void SetPaused(bool paused);
    void SetVolume(float volume);
    // Offline-only, interleaved stereo float PCM at 48 kHz.
    bool Render(float *samples, std::size_t frames);
private:
    struct State;
    std::shared_ptr<State> state_;
    friend class AudioClip;
};
class AudioClip {
public:
    ~AudioClip();
    AudioClip(const AudioClip &) = delete;
    AudioClip &operator=(const AudioClip &) = delete;
    // Repeated play updates an active sound without restarting it (XRSound API).
    bool Play(bool loop, float volume);
    bool PlayFrom(double seconds, bool loop, float volume);
    double Position() const; // seconds in the source file, or -1 on failure
    bool Stop();
    bool IsPlaying() const;
    void SetGain(float gain);
private:
    struct State;
    explicit AudioClip(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    friend class AudioMixer;
};
}
