#include "SDLJoystick.h"
#include "Joystick.h"
#include <stdexcept>
namespace orbiter::platform {
SDLJoysticks::SDLJoysticks() {
    if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK)) throw std::runtime_error(SDL_GetError());
}
SDLJoysticks::~SDLJoysticks() {
    PublishJoystickStates({});
    for (auto [id, device] : devices_) SDL_CloseJoystick(device);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}
void SDLJoysticks::Poll() {
    for (auto it = devices_.begin(); it != devices_.end();) {
        if (!SDL_JoystickConnected(it->second)) {
            SDL_CloseJoystick(it->second);
            it = devices_.erase(it);
        } else ++it;
    }
    int count = 0;
    auto ids = SDL_GetJoysticks(&count);
    for (int i = 0; i < count; ++i) {
        if (!devices_.count(ids[i])) {
            if (auto device = SDL_OpenJoystick(ids[i])) devices_.emplace(ids[i], device);
        }
    }
    SDL_free(ids);
    SDL_UpdateJoysticks();
    std::vector<JoystickState> states;
    for (auto [id, device] : devices_) {
        JoystickState state;
        state.instance = id;
        if (auto name = SDL_GetJoystickName(device)) state.name = name;
        if (auto serial = SDL_GetJoystickSerial(device)) state.serial = serial;
        char guid[33]{};
        SDL_GUIDToString(SDL_GetJoystickGUID(device), guid, sizeof guid);
        state.guid = guid;
        for (int i = 0; i < SDL_GetNumJoystickAxes(device); ++i) state.axes.push_back(SDL_GetJoystickAxis(device, i));
        for (int i = 0; i < SDL_GetNumJoystickButtons(device); ++i) state.buttons.push_back(SDL_GetJoystickButton(device, i));
        for (int i = 0; i < SDL_GetNumJoystickHats(device); ++i) state.hats.push_back(SDL_GetJoystickHat(device, i));
        states.push_back(std::move(state));
    }
    PublishJoystickStates(std::move(states));
}
}
