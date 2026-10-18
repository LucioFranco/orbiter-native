#include "Platform/SDLJoystick.h"
#include "Platform/Joystick.h"
#include <iostream>
#include <stdexcept>
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        {
            orbiter::platform::SDLJoysticks backend;
            SDL_VirtualJoystickDesc desc{};
            SDL_INIT_INTERFACE(&desc);
            desc.name = "NASSP virtual flight controller";
            desc.naxes = 3; desc.nbuttons = 2; desc.nhats = 1;
            const auto id = SDL_AttachVirtualJoystick(&desc);
            require(id != 0, "attach virtual joystick");
            auto joystick = SDL_OpenJoystick(id);
            require(joystick != nullptr, "open virtual joystick");
            backend.Poll();
            auto state = [&]() -> const orbiter::platform::JoystickState & {
                for (const auto &device : orbiter::platform::JoystickStates()) if (device.instance == id) return device;
                throw std::runtime_error("virtual controller absent from shared core");
            };
            require(state().name == desc.name && state().guid.size() == 32, "device identity");
            require(SDL_SetJoystickVirtualAxis(joystick, 0, -32768) &&
                SDL_SetJoystickVirtualAxis(joystick, 1, 32767) &&
                SDL_SetJoystickVirtualButton(joystick, 1, true) &&
                SDL_SetJoystickVirtualHat(joystick, 0, SDL_HAT_RIGHTUP), "set physical input equivalent");
            backend.Poll();
            require(state().axes.size() == 3 && state().axes[0] == -32768 && state().axes[1] == 32767, "full signed axis range");
            require(state().buttons[1] && state().hats[0] == SDL_HAT_RIGHTUP, "buttons and diagonal hat");
            require(SDL_SetJoystickVirtualButton(joystick, 1, false), "release button");
            backend.Poll(); require(!state().buttons[1], "button release published");
            SDL_CloseJoystick(joystick);
            require(SDL_DetachVirtualJoystick(id), "hot unplug");
            backend.Poll();
            for (const auto &device : orbiter::platform::JoystickStates()) require(device.instance != id, "unplug removes stale controls");
        }
        require(orbiter::platform::JoystickStates().empty(), "shutdown clears shared state");
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
