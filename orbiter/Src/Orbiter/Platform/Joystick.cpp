#include "Joystick.h"
#include <utility>
#include <map>
namespace orbiter::platform {
namespace {
std::vector<JoystickState> devices;
std::map<void *, void (*)(void *)> listeners;
bool inputEnabled = true;
}
void RegisterJoystickRelease(void *owner, void (*release)(void *)) { listeners[owner] = release; }
void UnregisterJoystickRelease(void *owner) { listeners.erase(owner); }
void ReleaseJoystickControls() {
    const auto current = listeners;
    for (auto [owner, release] : current) if (listeners.count(owner)) release(owner);
}
bool JoystickInputEnabled() { return inputEnabled; }
void SetJoystickInputEnabled(bool enabled) {
    if (inputEnabled && !enabled) ReleaseJoystickControls();
    inputEnabled = enabled;
}
const std::vector<JoystickState> &JoystickStates() { return devices; }
void PublishJoystickStates(std::vector<JoystickState> states) { devices = std::move(states); }
}
