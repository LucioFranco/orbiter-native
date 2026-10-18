#include "NativeSound.h"
#include "Orbiter.h"
#include "Vessel.h"
#include "Camera.h"
#include <filesystem>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
extern Camera *g_camera;
static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static double energy(const std::vector<float> &samples) {
    double value = 0; for (float sample : samples) { require(std::isfinite(sample), "finite audio output"); value += sample*sample; }
    return value / samples.size();
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "paired sound runtime required");
        std::filesystem::current_path(argv[1]);
        { std::unique_ptr<XRSound> absent(XRSound::CreateInstance(nullptr));
          require(!absent->IsPresent() && !absent->LoadWav(1,"Sound/ProjectApollo/Click.wav",XRSound::PlaybackType::Global), "null session remains unavailable"); }
        orbiter::NativeAudioSession audio(orbiter::platform::AudioMixer::Output::Offline);
        std::vector<float> samples(48000*2);
        auto render = [&]() { require(audio.Render(samples.data(),48000), "render sound output"); return energy(samples); };
        auto silent = [&]() {
            float boundary[16]{}; require(audio.Render(boundary,8), "drain bounded graph boundary");
            return render()<1e-10;
        };
        {
            std::unique_ptr<XRSound> a(XRSound::CreateInstance(nullptr)), b(XRSound::CreateInstance(nullptr));
            require(a->IsPresent() && b->IsPresent(), "native XRSound present");
            require(a->LoadWav(12000,"sound\\projectapollo\\lmsuitfan.WAV",XRSound::PlaybackType::Global), "XRSound VFS lookup");
            require(b->LoadWav(12000,"Sound/ProjectApollo/Click.wav",XRSound::PlaybackType::Global), "same ID in independent bank");
            require(!a->PlayWav(99) && !a->StopWav(99) && !a->IsWavPlaying(99), "unknown sound ID rejected");
            require(!a->PlayWavFrom(99,0) && a->GetWavPosition(99)<0,"unknown seek/cursor ID rejected");
            require(a->PlayWavFrom(12000,0.1) && std::abs(a->GetWavPosition(12000)-0.1)<0.001,
                    "XRSound starts mission audio at requested offset");
            require(render()>1e-8 && a->GetWavPosition(12000)>0.1,"seeked XRSound produces PCM and advances");
            require(a->StopWav(12000),"stop seeked recording");
            require(a->PlayWav(12000,true,0.5f) && render()>1e-8, "XRSound loop audible");
            require(a->IsWavPlaying(12000) && !b->IsWavPlaying(12000), "banks do not share playing state");
            require(!a->LoadWav(12000,"absent.wav",XRSound::PlaybackType::Global) && a->IsWavPlaying(12000), "failed replacement preserves active sound");
            require(a->LoadWav(12000,"Sound/ProjectApollo/Click.wav",XRSound::PlaybackType::Global) && !a->IsWavPlaying(12000), "successful replacement stops previous loop");
            require(silent(), "replaced loop detached");
            require(b->PlayWav(12000,false) && render()>1e-8 && !b->IsWavPlaying(12000), "one shot completes");
        }
        require(silent(), "sound bank destruction leaves silence");
        {
            Orbiter host; host.Launch("NasspPair");
            auto csm = reinterpret_cast<Vessel *>(oapiGetVesselByName("Columbia"));
            auto lm = reinterpret_cast<Vessel *>(oapiGetVesselByName("Eagle"));
            require(csm && lm, "original spacecraft loaded with sound enabled");
            g_camera->Attach(csm,0);
            render(); require(silent(), "cockpit quiet before simulation starts");
            Vector point(0.5,0.5,0);
            require(csm->VCMouseEvent(3000,PANEL_MOUSE_LBDOWN,point), "original CSM seat control callback");
            require(render()>1e-8, "original CSM SwitchClick produces PCM");
            require(silent(), "CSM click ends");
            // Trigger the same internal-only sound while another vessel has focus.
            host.SetFocusObject(lm,false); g_camera->Attach(lm,0);
            require(csm->VCMouseEvent(3000,PANEL_MOUSE_LBDOWN,point), "unfocused CSM click callback");
            require(silent(), "unfocused internal sound is inaudible");
            host.NativeKeyEvent(OAPI_KEY_COMMA,true); host.NativeKeyEvent(OAPI_KEY_COMMA,false);
            require(render()>1e-8, "original LM ButtonClick produces PCM");
            require(silent(), "LM click ends");
            g_camera->Attach(lm,1);
            host.NativeKeyEvent(OAPI_KEY_COMMA,true); host.NativeKeyEvent(OAPI_KEY_COMMA,false);
            require(silent(), "internal click muted in external view");
            g_camera->Attach(lm,0);
            std::unique_ptr<XRSound> loop(XRSound::CreateInstance(oapiGetFocusInterface()));
            require(loop->LoadWav(1,"Sound/ProjectApollo/LMSuitFan.wav",XRSound::PlaybackType::InternalOnly) && loop->PlayWav(1,true), "pause probe playing");
            require(render()>1e-8, "focused loop audible");
            host.Pause(true); host.StepSimulation(1.0/60);
            require(silent(), "simulator pause silences mixer");
            host.Pause(false); host.StepSimulation(1.0/60);
            require(render()>1e-8, "simulator resume restores audio");
        }
        require(silent(), "vessel shutdown releases every sound bank");
        std::cout << "Original CSM/LM sound events, bank isolation, focus/view policy and pause verified\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
