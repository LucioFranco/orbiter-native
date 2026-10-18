#include "Orbiter.h"
#include "Platform/SDLJoystick.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
int main(int argc, char **argv) {
    std::filesystem::path config;
    bool configCreated = false;
    try {
        require(argc == 2, "isolated CSM runtime required");
        std::filesystem::current_path(argv[1]);
        config = "Config/ProjectApollo/Vesim/CSM.native.cfg";
        require(!std::filesystem::exists(config), "controller fixture must not overwrite a config");
        configCreated = true;
        {
            std::ofstream out(config);
            out << "\"RHC Roll\" \"NASSP flight stick\" \"\" Axis 0 0 0.05\n"
                "\"RHC Pitch\" \"NASSP flight stick\" \"\" Axis 1 0 0.05\n"
                "\"RHC Yaw\" \"NASSP flight stick\" \"\" Axis 2 0 0.05\n";
        }
        Orbiter host; host.Launch("NasspCSM");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        orbiter::platform::SDLJoysticks backend;
        SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc);
        desc.name = "NASSP flight stick"; desc.naxes = 3;
        auto id = SDL_AttachVirtualJoystick(&desc); auto stick = SDL_OpenJoystick(id);
        require(stick, "open virtual flight stick");
        auto vessel = oapiGetFocusInterface();
        auto step = [&](int n) { for (int i = 0; i < n; ++i) { backend.Poll(); host.StepSimulation(1.0 / 60); } };
        auto force = [&](bool moment) {
            VECTOR3 result{};
            for (DWORD i = 0; i < vessel->GetThrusterCount(); ++i) {
                auto thruster = vessel->GetThrusterHandleByIndex(i); VECTOR3 direction{}, position{};
                vessel->GetThrusterDir(thruster, direction); vessel->GetThrusterRef(thruster, position);
                const auto thrust = direction * (vessel->GetThrusterLevel(thruster) * vessel->GetThrusterMax(thruster));
                result += moment ? crossp(thrust, position) : thrust;
            }
            return result;
        };
        step(120);
        const VECTOR3 translation[] = {_V(1,0,0), _V(0,1,0), _V(0,0,1)};
        const VECTOR3 rotation[] = {_V(0,0,1), _V(1,0,0), _V(0,-1,0)};
        for (bool rotate : {false, true}) for (int axis = 0; axis < 3; ++axis) for (int sign : {-1, 1}) {
            vessel->SetAttitudeMode(rotate ? RCS_ROT : RCS_LIN);
            const double fuel = vessel->GetTotalPropellantMass();
            require(SDL_SetJoystickVirtualAxis(stick, axis, sign < 0 ? -32768 : 32767), "deflect stick");
            VECTOR3 impulse{};
            for (int i = 0; i < 30; ++i) { step(1); impulse += force(rotate) / 60.0; }
            require(dotp(impulse, (rotate ? rotation[axis] : translation[axis]) * sign) > 10,
                "joystick produces thrust/torque along commanded axis");
            require(vessel->GetTotalPropellantMass() < fuel - 0.01, "joystick firing consumes spacecraft propellant");
            require(SDL_SetJoystickVirtualAxis(stick, axis, 0), "center stick");
            vessel->SetAttitudeMode(RCS_LIN); step(120);
            require(length(force(false)) < 1 && length(force(true)) < 1, "jets settle after stick release");
            std::cout << (rotate ? "rotation" : "translation") << " axis " << axis << " sign " << sign << " verified\n";
        }
        SDL_CloseJoystick(stick); SDL_DetachVirtualJoystick(id);
        std::filesystem::remove(config);
        return 0;
    } catch (const std::exception &error) {
        if (configCreated) { std::error_code ignored; std::filesystem::remove(config, ignored); }
        std::cerr << error.what() << '\n'; return 1;
    }
}
