#include "NativeSound.h"
#include "Orbitersdk.h"
#include "Camera.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
extern Camera *g_camera;
namespace {
std::shared_ptr<orbiter::platform::AudioMixer> activeMixer;
std::set<XRSound *> banks;
}
struct XRSound::State {
    struct Sound {
        std::unique_ptr<orbiter::platform::AudioClip> clip;
        PlaybackType type;
    };
    VESSEL *vessel;
    std::shared_ptr<orbiter::platform::AudioMixer> mixer;
    std::map<int, Sound> sounds;
};
namespace orbiter {
NativeAudioSession::NativeAudioSession(platform::AudioMixer::Output output) {
    if (activeMixer) throw std::logic_error("Native audio session already active");
    mixer_ = std::make_shared<platform::AudioMixer>(output);
    activeMixer = mixer_;
}
NativeAudioSession::~NativeAudioSession() {
    mixer_->SetPaused(true);
    if (activeMixer == mixer_) activeMixer.reset();
}
bool NativeAudioSession::Render(float *samples, std::size_t frames) {
    UpdateNativeAudio();
    return mixer_->Render(samples, frames);
}
void NativeAudioSession::SetVolume(float volume) { mixer_->SetVolume(volume); }
void UpdateNativeAudio() { for (auto bank : banks) bank->Update(); }
void PauseNativeAudio(bool paused) { if (activeMixer) activeMixer->SetPaused(paused); }
}
XRSound::XRSound(VESSEL *vessel) : state_(std::make_unique<State>()) {
    state_->vessel = vessel; state_->mixer = activeMixer; banks.insert(this);
}
XRSound *XRSound::CreateInstance(VESSEL *vessel) { return new XRSound(vessel); }
XRSound::~XRSound() { banks.erase(this); }
bool XRSound::IsPresent() const { return bool(state_->mixer); }
bool XRSound::SetDefaultSoundEnabled(DefaultSound, bool enabled) {
    // Native audio has no automatically generated Orbiter engine/callout sounds.
    return IsPresent() && !enabled;
}
bool XRSound::LoadWav(int id, const char *path, PlaybackType type) {
    if (!IsPresent() || id < 0 || !path || !*path) return false;
    try {
        auto clip = state_->mixer->Load(path);
        if (!clip) return false;
        state_->sounds.insert_or_assign(id, State::Sound{std::move(clip), type});
        Update();
        return true;
    } catch (const std::exception &) { return false; }
}
bool XRSound::PlayWav(int id, bool loop, float volume) {
    const auto sound = state_->sounds.find(id);
    if (sound == state_->sounds.end()) return false;
    Update();
    return sound->second.clip->Play(loop, volume);
}
bool XRSound::PlayWavFrom(int id, double seconds, bool loop, float volume) {
    const auto sound=state_->sounds.find(id);
    if(sound==state_->sounds.end())return false;
    Update();return sound->second.clip->PlayFrom(seconds,loop,volume);
}
double XRSound::GetWavPosition(int id) const {
    const auto sound=state_->sounds.find(id);
    return sound==state_->sounds.end()?-1:sound->second.clip->Position();
}
bool XRSound::StopWav(int id) {
    const auto sound = state_->sounds.find(id);
    return sound != state_->sounds.end() && sound->second.clip->Stop();
}
bool XRSound::IsWavPlaying(int id) const {
    const auto sound = state_->sounds.find(id);
    return sound != state_->sounds.end() && sound->second.clip->IsPlaying();
}
void XRSound::Update() {
    const bool focused = state_->vessel && state_->vessel->GetHandle() == oapiGetFocusObject();
    const bool internal = g_camera && oapiCameraInternal();
    for (auto &[id, sound] : state_->sounds) {
        float gain = 0;
        if (sound.type == PlaybackType::Global) gain = 1;
        else if (sound.type == PlaybackType::Radio) gain = focused ? 1 : 0;
        else if (sound.type == PlaybackType::InternalOnly) gain = focused && internal ? 1 : 0;
        else if (sound.type != PlaybackType::Wind && focused && internal) gain = 1;
        else if (!internal && state_->vessel && g_camera) {
            const double pressure = state_->vessel->GetAtmPressure();
            double attenuation = std::clamp(pressure / 101325.0, 0.0, 1.0);
            if (sound.type != PlaybackType::Wind) {
                VECTOR3 vessel{}, camera{};
                state_->vessel->GetGlobalPos(vessel); oapiCameraGlobalPos(&camera);
                const double range = sound.type == PlaybackType::BothViewClose ? 100 :
                    sound.type == PlaybackType::BothViewMedium ? 1000 : 10000;
                const double distance = length(vessel - camera) / range;
                attenuation /= 1 + distance * distance;
            }
            gain = std::isfinite(attenuation) ? static_cast<float>(attenuation) : 0;
        }
        sound.clip->SetGain(gain);
    }
}
