#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace orbiter::platform {
// Main-thread snapshots shared by the host and native addons through liborbiter.
// Axes retain SDL's signed [-32768,32767] range; hats use SDL's direction bits.
struct JoystickState {
    std::uint32_t instance = 0;
    std::string name, guid, serial;
    std::vector<std::int16_t> axes;
    std::vector<std::uint8_t> buttons, hats;
};
const std::vector<JoystickState> &JoystickStates();
// Release listeners are main-thread-only and must unregister before destruction.
void RegisterJoystickRelease(void *owner, void (*release)(void *));
void UnregisterJoystickRelease(void *owner);
void ReleaseJoystickControls();
bool JoystickInputEnabled();
void SetJoystickInputEnabled(bool enabled);
void PublishJoystickStates(std::vector<JoystickState> states);
}
