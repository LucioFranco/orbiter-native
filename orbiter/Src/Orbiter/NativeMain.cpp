// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#include "Orbiter.h"
#include "CoreState.h"
#include "Log.h"
#include "NativeSound.h"
#include "Platform/SDLWindow.h"
#include "Platform/RealtimeSteps.h"
#include <memory>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    bool showErrors=false;
#ifdef ORBITER_POC_ASSET_ROOT
    showErrors=true;
#endif
    try {
        std::string scenario, root, graphics, save;
        std::vector<std::string> modules;
#ifdef ORBITER_POC_MFD
        modules.emplace_back("ProjectApolloMFD");
#endif
        uint64_t steps = 600;
        bool explicitSteps = false, realtime = false;
        int sound = -1;
        double dt = 1.0 / 60.0;
        bool windowed = false, cockpit = false, profile = false, laptopControls=false;
        double stepTime=0,renderTime=0,frameTime=0;uint64_t samples=0;
#ifdef ORBITER_POC_ASSET_ROOT
        scenario="NasspCSM";root=ORBITER_POC_ASSET_ROOT;
        graphics="WgpuClient";windowed=cockpit=laptopControls=true;
#endif
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--laptop-controls") {laptopControls=true;continue;}
            if (option == "--realtime") {realtime=true;continue;}
            if (option == "--sound") {sound=1;continue;}
            if (option == "--no-sound") {sound=0;continue;}
            if (option == "--profile") {profile=true;continue;}
            if (option == "--vc") { cockpit = true; continue; }
            if (option == "--window") { windowed = showErrors = true; continue; }
            if (option == "--headless") { windowed = showErrors = false; continue; }
            if (option == "--help") {
                std::cout << "orbiter --scenario <path> [--root <assets>] [--steps <count>] [--dt <seconds>] [--window|--headless] [--vc] [--graphics <module>] [--module <plugin>] [--save <scenario>] [--profile] [--realtime] [--laptop-controls] [--sound|--no-sound]\n"
                             "Native scenario runner; --window enables SDL input, use --graphics WgpuClient for native GPU output.\n"
                             "Windowed sessions run at wall-clock speed until closed; headless sessions default to 600 steps.\n"
                             "--steps runs a fixed count deterministically; add --realtime for a paced windowed run.\n";
                return 0;
            }
            if (++i >= argc) throw std::invalid_argument("Missing value for " + option);
            const std::string value = argv[i];
            if (option == "--scenario") scenario = value;
            else if (option == "--root") root = value;
            else if (option == "--graphics") graphics = value;
            else if (option == "--module") modules.push_back(value);
            else if (option == "--save") save = value;
            else if (option == "--steps") {
                explicitSteps = true;
                auto result = std::from_chars(value.data(), value.data() + value.size(), steps);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                    throw std::invalid_argument("Invalid step count");
            } else if (option == "--dt") {
                std::size_t count = 0;
                dt = std::stod(value, &count);
                if (count != value.size() || !(dt > 0) || !std::isfinite(dt))
                    throw std::invalid_argument("Invalid timestep");
            } else throw std::invalid_argument("Unknown option: " + option);
        }
        if(realtime && !windowed)throw std::invalid_argument("--realtime requires --window");
        if (scenario.empty()) throw std::invalid_argument("Use --scenario <path>");
        if (!root.empty()) std::filesystem::current_path(root);
        InitLog("Orbiter.log", false);
        std::unique_ptr<orbiter::NativeAudioSession> audio;
        if (sound == 1 || (sound < 0 && windowed)) {
            try { audio = std::make_unique<orbiter::NativeAudioSession>(orbiter::platform::AudioMixer::Output::Device); }
            catch (const std::exception &error) { std::cerr << "Audio unavailable: " << error.what() << '\n'; }
        }
        Orbiter host;
        if (!graphics.empty()) {
            if (!host.LoadModule("Modules/Plugin", graphics.c_str()) || !host.GetGraphicsClient())
                throw std::runtime_error("Could not load graphics client " + graphics);
            if (!host.GetGraphicsClient()->clbkInitialise())
                throw std::runtime_error("Graphics client initialization failed");
        }
        for(const auto &module:modules)
            if(!host.LoadModule("Modules/Plugin",module.c_str()))throw std::runtime_error("Could not load plugin " + module);
        host.Launch(scenario.c_str());
        if (cockpit && !host.LoadNativeVirtualCockpit()) throw std::runtime_error("Vessel virtual cockpit unavailable");
        std::unique_ptr<orbiter::platform::SDLWindow> window;
        if (windowed) window = std::make_unique<orbiter::platform::SDLWindow>(host,false,laptopControls);
        realtime=realtime || (windowed && !explicitSteps);
        orbiter::platform::RealtimeSteps clock(dt);
        auto previous=SDL_GetTicksNS();
        uint64_t completed = 0,frames=0;
        for (; (!explicitSteps && windowed) || completed < steps; ++frames) {
            const auto start = SDL_GetTicksNS();
            if (window && !window->Poll()) break;
            const auto stepStart=SDL_GetTicksNS();
            unsigned due=1;
            if(realtime) {due=clock.Advance(double(stepStart-previous)/1e9);previous=stepStart;}
            for(unsigned tick=0;tick<due && ((!explicitSteps && windowed) || completed<steps);++tick) {
                host.StepSimulation(dt);if(host.IsRunning())++completed;
            }
            const auto renderStart=SDL_GetTicksNS();
            host.RenderNativeFrame();
            const auto renderEnd=SDL_GetTicksNS();
            if (window) {
                const auto elapsed = SDL_GetTicksNS() - start;
                const auto frame = static_cast<Uint64>((realtime?std::min(dt,1.0/60.0):dt) * 1e9);
                if (elapsed < frame) SDL_DelayNS(frame - elapsed);
            }
            if(profile && frames>=10) {
                stepTime+=double(renderStart-stepStart)/1e6;
                renderTime+=double(renderEnd-renderStart)/1e6;
                frameTime+=double(SDL_GetTicksNS()-start)/1e6;++samples;
            }
        }
        if(profile && samples)std::cout<<"Profile after 10 warmup frames: samples="<<samples
            <<" step_ms="<<stepTime/samples<<" render_ms="<<renderTime/samples
            <<" frame_ms="<<frameTime/samples<<'\n';
        if (!save.empty()) {
            if (!host.SaveScenario(save.c_str(),"Saved native Orbiter / NASSP session",0))
                throw std::runtime_error("Cannot save scenario: " + save);
            std::cout << "Saved scenario " << save << '\n';
        }
        std::cout << "Completed " << completed << (windowed ? " windowed steps; simulation time " : " headless steps; simulation time ") << td.SimT0 << " s\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "orbiter: " << error.what() << '\n';
        if(showErrors)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Orbiter / NASSP",error.what(),nullptr);
        return 1;
    }
}
