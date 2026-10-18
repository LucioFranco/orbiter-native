#include "Orbiter.h"
#include "Vessel.h"
#include "Platform/SDLWindow.h"
#include "Platform/Joystick.h"
#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static int pitch(Vessel *vessel) {
    std::ostringstream out; vessel->Write(out);
    const auto state = out.str(); const auto offset = state.find("VICHAN025 ");
    require(offset != std::string::npos, "AGC pitch input channel saved");
    return std::stoi(state.substr(offset + 10)) & 3;
}
struct Configs {
    std::filesystem::path csm = "Config/ProjectApollo/Vesim/CSM.native.cfg";
    std::filesystem::path lm = "Config/ProjectApollo/Vesim/LM.native.cfg";
    Configs(bool throttle) {
        require(!std::filesystem::exists(csm) && !std::filesystem::exists(lm), "isolated controller fixture must not contain user configs");
        std::ofstream(csm) << "\"RHC Pitch\" \"NASSP test stick\" \"\" Axis 0 0 0.05\n"
            "\"Rotation/Translation toggle\" \"NASSP test stick\" \"\" Button 0\n";
        std::ofstream(lm) << "\"ACA Pitch\" \"NASSP test stick\" \"\" Axis 0 0 0.05\n"
            "\"Rotation/Translation toggle\" \"NASSP test stick\" \"\" Button 0\n";
        if (throttle) std::ofstream(lm, std::ios::app) << "\"TTCA Throttle\" \"NASSP test stick\" \"\" Axis 1 0 0\n";
    }
    ~Configs() { std::filesystem::remove(csm); std::filesystem::remove(lm); }
};
int main(int argc, char **argv) {
    try {
        require(argc == 2 || argc == 3, "isolated paired runtime required");
        const bool throttle = argc == 3;
        std::filesystem::current_path(argv[1]); Configs configs(throttle);
        // Configure the original physical switches for a manual DPS probe.
        const auto scenarioPath = std::filesystem::path("Scenarios/NasspPair.scn");
        std::ifstream scenarioIn(scenarioPath);
        std::string scenario((std::istreambuf_iterator<char>(scenarioIn)), {});
        scenarioIn.close();
        for (const std::string name : {"EngineArmSwitch", "THRContSwitch"}) {
            const auto at = scenario.find("  " + name + " 1 0");
            require(at != std::string::npos, "fixture switch available");
            scenario.replace(at, name.size() + 6, "  " + name + " 0 0");
        }
        { std::ofstream out(scenarioPath); out << scenario; }
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        Orbiter host; host.Launch("NasspPair");
        auto csm = reinterpret_cast<Vessel *>(oapiGetVesselByName("Columbia"));
        auto lm = reinterpret_cast<Vessel *>(oapiGetVesselByName("Eagle"));
        require(csm && lm, "both spacecraft loaded");
        orbiter::platform::SDLWindow window(host, true);
        SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc);
        desc.name = "NASSP test stick"; desc.naxes = 2; desc.nbuttons = 1;
        auto id = SDL_AttachVirtualJoystick(&desc); require(id, "attach virtual flight stick");
        auto stick = SDL_OpenJoystick(id); require(stick, "open virtual flight stick");
        auto axis = [&](int value) { require(SDL_SetJoystickVirtualAxis(stick, 0, value), "set stick axis"); };
        auto button = [&](bool value) { require(SDL_SetJoystickVirtualButton(stick, 0, value), "set stick button"); };
        auto step = [&](int n = 2) { for (int i = 0; i < n; ++i) { require(window.Poll(), "SDL poll"); host.StepSimulation(1.0 / 60); } };
        auto focus = [&](bool active) { SDL_Event e{}; e.type = active ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST; require(SDL_PushEvent(&e), "window focus event"); step(); };
        focus(true); step(60);
        oapiGetFocusInterface()->SetAttitudeMode(RCS_ROT);
        axis(25000); step(); require(pitch(csm) != 3, "SDL joystick reaches original CSM RHC and AGC");
        host.SetFocusObject(lm, false);
        require(pitch(csm) == 3, "CSM focus loss immediately clears hand controller");
        oapiGetFocusInterface()->SetAttitudeMode(RCS_ROT);
        step(); require(pitch(lm) == 3, "held axis cannot transfer to Eagle");
        axis(0); step(); axis(25000); step(); require(pitch(lm) != 3, "fresh LM ACA command reaches LGC");
        focus(false); require(pitch(lm) == 3, "window focus loss releases ACA");
        focus(true); require(pitch(lm) == 3, "window refocus requires centered stick");
        axis(0); step(); axis(25000); step(); require(pitch(lm) != 3, "LM stick rearms");
        host.Pause(true); step(); host.Pause(false); step(3);
        require(pitch(lm) == 3, "pause resume cannot restore held command");
        axis(0); step(); axis(25000); step();
        ImGui::GetIO().WantCaptureKeyboard = true; step();
        require(pitch(lm) == 3, "UI capture releases joystick");
        ImGui::GetIO().WantCaptureKeyboard = false; step();
        require(pitch(lm) == 3, "leaving UI requires recenter");
        axis(0); step(); axis(25000); step();
        SDL_CloseJoystick(stick); require(SDL_DetachVirtualJoystick(id), "detach flight stick"); step();
        require(pitch(lm) == 3, "hot unplug releases active ACA");
        id = SDL_AttachVirtualJoystick(&desc); stick = SDL_OpenJoystick(id); require(stick, "reconnect stick");
        axis(25000); button(true); step();
        require(pitch(lm) == 3 && oapiGetFocusInterface()->GetAttitudeMode() == RCS_ROT, "held reconnected controls do not command vessel");
        axis(0); button(false); step(); button(true); step();
        require(oapiGetFocusInterface()->GetAttitudeMode() == RCS_LIN, "mapped button invokes real LM mode callback");
        step(); require(oapiGetFocusInterface()->GetAttitudeMode() == RCS_LIN, "held button toggles only once");
        button(false); step();
        host.SetFocusObject(csm, false); oapiGetFocusInterface()->SetAttitudeMode(RCS_ROT); step();
        button(true); step(); require(oapiGetFocusInterface()->GetAttitudeMode() == RCS_LIN, "mapped button invokes CSM callback");
        button(false); step();
        // A connected mapped axis owns pitch; disconnect restores keyboard input.
        oapiGetFocusInterface()->SetAttitudeMode(RCS_ROT);
        host.NativeKeyEvent(OAPI_KEY_NUMPAD2, true); // mapped pitch intentionally remains centered
        step(); require(pitch(csm) == 3, "active mapped axis owns its assigned input");
        host.ReleaseNativeKeys();
        SDL_CloseJoystick(stick); SDL_DetachVirtualJoystick(id); step();
        host.NativeKeyEvent(OAPI_KEY_NUMPAD2, true); step();
        require(pitch(csm) != 3, "disconnected mapped axis restores keyboard fallback");
        host.ReleaseNativeKeys(); step();
        host.SetFocusObject(lm, false);
        id = SDL_AttachVirtualJoystick(&desc); stick = SDL_OpenJoystick(id); require(stick, "attach throttle test stick");
        axis(0); step(5);
        auto sdk = oapiGetFocusInterface();
        THRUSTER_HANDLE engine = nullptr;
        for (DWORD i = 0; i < sdk->GetThrusterCount(); ++i) {
            auto candidate = sdk->GetThrusterHandleByIndex(i);
            if (sdk->GetThrusterMax(candidate) > 40000) engine = candidate;
        }
        require(engine, "LM descent engine exists");
        host.NativeKeyEvent(OAPI_KEY_ADD, true); step(30);
        host.NativeKeyEvent(OAPI_KEY_ADD, false); step(120);
        const double low = sdk->GetThrusterLevel(engine);
        require(low > 0.05, "manual DPS ignition with VESIM enabled");
        if (throttle) {
            require(SDL_SetJoystickVirtualAxis(stick, 1, -25000), "increase mapped throttle"); step(120);
        } else {
            host.NativeKeyEvent(OAPI_KEY_NUMPAD0, true); step(180);
            host.NativeKeyEvent(OAPI_KEY_NUMPAD0, false); step(60);
        }
        const double high = sdk->GetThrusterLevel(engine);
        require(high > low + 0.2, "mapped or unmapped keyboard throttle increases actual DPS thrust");
        SDL_CloseJoystick(stick); SDL_DetachVirtualJoystick(id); step(120);
        require(std::abs(sdk->GetThrusterLevel(engine) - high) < 0.01, "unplug preserves manual throttle position");
        host.NativeKeyEvent(OAPI_KEY_DECIMAL, true); step(120);
        host.NativeKeyEvent(OAPI_KEY_DECIMAL, false); step(60);
        require(sdk->GetThrusterLevel(engine) < high - 0.1, "keyboard throttle available after disconnect");
        host.NativeKeyEvent(OAPI_KEY_SUBTRACT, true); step(30);
        host.NativeKeyEvent(OAPI_KEY_SUBTRACT, false); step(60);
        require(sdk->GetThrusterLevel(engine) < 0.001, "DPS shuts down after controller flight");
        std::cout << "SDL joystick verified through CSM/LM hand controllers, callbacks and focus/pause/unplug transitions\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
