#include "Audio.h"
#include "Vfs.h"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace orbiter::platform {
namespace {
float Level(float value) { return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f; }
}
struct AudioMixer::State {
    ma_engine engine{};
    ma_sound_group group{};
    bool initialized = false, groupInitialized = false, offline = false;
    ~State() {
        if (groupInitialized) ma_sound_group_uninit(&group);
        if (initialized) ma_engine_uninit(&engine);
    }
};
struct AudioClip::State {
    // Keep the mixer alive until all its loaded sounds have been released.
    std::shared_ptr<AudioMixer::State> mixer;
    ma_sound sound{};
    bool initialized = false;
    float volume = 1, gain = 1;
    ~State() { if (initialized) ma_sound_uninit(&sound); }
};
AudioMixer::AudioMixer(Output output) : state_(std::make_shared<State>()) {
    auto config = ma_engine_config_init();
    state_->offline = output == Output::Offline;
    config.noDevice = state_->offline;
    config.channels = 2;
    config.sampleRate = 48000;
    if (ma_engine_init(&config, &state_->engine) != MA_SUCCESS) throw std::runtime_error("Cannot initialize native audio output");
    state_->initialized = true;
    if (ma_sound_group_init(&state_->engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &state_->group) != MA_SUCCESS)
        throw std::runtime_error("Cannot initialize native audio mixer group");
    state_->groupInitialized = true;
}
AudioMixer::~AudioMixer() = default;
std::unique_ptr<AudioClip> AudioMixer::Load(std::string_view path) {
    auto clip = std::make_unique<AudioClip::State>();
    clip->mixer = state_;
    const auto resolved = ResolvePath(path).string();
    if (ma_sound_init_from_file(&state_->engine, resolved.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
        &state_->group, nullptr, &clip->sound) != MA_SUCCESS) return nullptr;
    clip->initialized = true;
    return std::unique_ptr<AudioClip>(new AudioClip(std::move(clip)));
}
void AudioMixer::SetPaused(bool paused) {
    if (paused) ma_sound_group_stop(&state_->group);
    else ma_sound_group_start(&state_->group);
}
void AudioMixer::SetVolume(float volume) { ma_engine_set_volume(&state_->engine, Level(volume)); }
bool AudioMixer::Render(float *samples, std::size_t frames) {
    if (!state_->offline || !samples || frames > std::numeric_limits<std::size_t>::max() / (2 * sizeof(float))) return false;
    ma_uint64 read = 0;
    return ma_engine_read_pcm_frames(&state_->engine, samples, frames, &read) == MA_SUCCESS && read == frames;
}
AudioClip::AudioClip(std::unique_ptr<State> state) : state_(std::move(state)) {}
AudioClip::~AudioClip() = default;
bool AudioClip::Play(bool loop, float volume) {
    state_->volume = Level(volume);
    ma_sound_set_volume(&state_->sound, state_->volume * state_->gain);
    ma_sound_set_looping(&state_->sound, loop);
    if (IsPlaying()) return true;
    if (ma_sound_seek_to_pcm_frame(&state_->sound, 0) != MA_SUCCESS) return false;
    return ma_sound_start(&state_->sound) == MA_SUCCESS;
}
bool AudioClip::PlayFrom(double seconds, bool loop, float volume) {
    if(!std::isfinite(seconds) || seconds<0)return false;
    ma_uint32 rate=0;ma_uint64 frames=0;
    if(ma_sound_get_data_format(&state_->sound,nullptr,nullptr,&rate,nullptr,0)!=MA_SUCCESS || !rate ||
       ma_sound_get_length_in_pcm_frames(&state_->sound,&frames)!=MA_SUCCESS)return false;
    const double frame=std::floor(seconds*rate);
    if(!std::isfinite(frame) || frame>=double(frames))return false;
    if(ma_sound_stop(&state_->sound)!=MA_SUCCESS ||
       ma_sound_seek_to_pcm_frame(&state_->sound,static_cast<ma_uint64>(frame))!=MA_SUCCESS)return false;
    state_->volume=Level(volume);
    ma_sound_set_volume(&state_->sound,state_->volume*state_->gain);
    ma_sound_set_looping(&state_->sound,loop);
    return ma_sound_start(&state_->sound)==MA_SUCCESS;
}
double AudioClip::Position() const {
    ma_uint32 rate=0;ma_uint64 cursor=0;
    if(ma_sound_get_data_format(&state_->sound,nullptr,nullptr,&rate,nullptr,0)!=MA_SUCCESS || !rate ||
       ma_sound_get_cursor_in_pcm_frames(&state_->sound,&cursor)!=MA_SUCCESS)return -1;
    return double(cursor)/rate;
}
bool AudioClip::Stop() {
    if (ma_sound_stop(&state_->sound) != MA_SUCCESS) return false;
    return ma_sound_seek_to_pcm_frame(&state_->sound, 0) == MA_SUCCESS;
}
bool AudioClip::IsPlaying() const {
    return ma_sound_is_playing(&state_->sound) && !ma_sound_at_end(&state_->sound);
}
void AudioClip::SetGain(float gain) {
    state_->gain = Level(gain);
    ma_sound_set_volume(&state_->sound, state_->volume * state_->gain);
}
}
