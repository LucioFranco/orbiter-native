#pragma once
#include <SDL3/SDL.h>
#include <map>
namespace orbiter::platform {
// Owns handles only; snapshots live in the shared core, not in SDL-linked addons.
class SDLJoysticks {
public:
    SDLJoysticks();
    ~SDLJoysticks();
    SDLJoysticks(const SDLJoysticks &) = delete;
    SDLJoysticks &operator=(const SDLJoysticks &) = delete;
    void Poll();
private:
    std::map<SDL_JoystickID, SDL_Joystick *> devices_;
};
}
