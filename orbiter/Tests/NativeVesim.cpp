#include "Orbitersdk.h"
#include "vesim.h"
#include "Platform/Joystick.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
using Event = std::tuple<int, int, int>;
static void callback(int id, int type, int value, void *context) {
    static_cast<std::vector<Event> *>(context)->emplace_back(id, type, value);
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "config output path required");
        const std::filesystem::path path = argv[1];
        std::vector<Event> events;
        Vesim vesim(callback, &events);
        require(vesim.addInput(1, "RHC Roll", VESIM_INPUTTYPE_AXIS, 32768, true), "register axis");
        require(vesim.addInput(2, "Main DSKY PRO", VESIM_INPUTTYPE_BUTTON, 0, true), "register button");
        require(vesim.addInput(3, "THC Move CW", VESIM_INPUTTYPE_BUTTON, 0, true), "register hat");
        require(vesim.addInput(4, "TTCA Throttle", VESIM_INPUTTYPE_AXIS, 0), "register unmapped throttle");
        require(!vesim.addInput(5, "RHC Roll", VESIM_INPUTTYPE_AXIS), "duplicate name rejected");
        auto write = [&](const char *content) { std::ofstream file(path); file << content; require(bool(file), "write config"); };
        write("# native mappings\n\"RHC Roll\" \"test-stick\" \"serial-1\" Axis 0 1 0.1\n"
              "\"Main DSKY PRO\" \"test-stick\" \"serial-1\" Button 0\n"
              "\"THC Move CW\" \"test-stick\" \"serial-1\" Hat 0 2 # right\n");
        require(vesim.loadConfig(path.string()), "load named bindings");
        orbiter::platform::JoystickState device;
        device.instance = 10; device.name = "test-stick"; device.serial = "serial-1";
        device.axes = {32767}; device.buttons = {1}; device.hats = {2};
        auto poll = [&]() { orbiter::platform::PublishJoystickStates({device}); vesim.poolDevices(); };
        poll();
        require(!vesim.hasActiveBinding(1) && !vesim.hasActiveBinding(2) && events.empty(), "held controls require neutral on initial attach");
        device.axes[0] = 0; device.buttons[0] = 0; device.hats[0] = 0; poll();
        require(vesim.hasActiveBinding(1) && vesim.hasActiveBinding(2), "neutral arms controls");
        require(!vesim.hasActiveBinding(4) && vesim.getInputValue(4) == 0, "unmapped throttle remains distinguishable from active zero");
        device.axes[0] = 2000; poll(); require(vesim.getInputValue(1) == 32768, "axis deadzone");
        device.axes[0] = -32768; device.buttons[0] = 1; device.hats[0] = 3; poll();
        require(vesim.getInputValue(1) == 65536 && vesim.getInputValue(2) == 1 && vesim.getInputValue(3) == 1, "inverted endpoint and button/diagonal hat");
        const auto count = events.size(); poll(); require(events.size() == count, "held button has no repeated callback");
        device.axes[0] = 32767; poll(); require(vesim.getInputValue(1) == 0, "positive endpoint");
        vesim.poolDevices(false);
        require(vesim.getInputValue(1) == 32768 && vesim.getInputValue(2) == 0 && !vesim.hasActiveBinding(2), "focus release neutralizes state");
        require(events.back() == Event(3, VESIM_EVTTYPE_BUTTON_OFF, 0), "release emits button off");
        poll(); require(!vesim.hasActiveBinding(1) && !vesim.hasActiveBinding(2), "held controls cannot transfer across focus");
        device.axes[0] = 0; device.buttons[0] = 0; device.hats[0] = 0; poll();
        device.buttons[0] = 1; poll(); require(vesim.getInputValue(2) == 1, "fresh press after neutral");
        orbiter::platform::PublishJoystickStates({}); vesim.poolDevices();
        require(vesim.getInputValue(2) == 0 && !vesim.hasActiveBinding(2), "disconnect releases button");
        device.instance = 11; poll(); require(!vesim.hasActiveBinding(2), "reconnect held button stays released");
        device.buttons[0] = 0; poll();
        auto duplicate = device; duplicate.instance = 12;
        orbiter::platform::PublishJoystickStates({device, duplicate}); vesim.poolDevices();
        require(!vesim.hasActiveBinding(1), "ambiguous identity cannot select arbitrary device");
        write("\"RHC Roll\" \"*\" \"\" Axis 0 0 1\n");
        bool rejected = false;
        try { vesim.loadConfig(path.string()); } catch (const std::runtime_error &error) {
            rejected = std::string(error.what()).find(":1:") != std::string::npos;
        }
        require(rejected, "invalid deadzone reports line");
        poll(); device.axes[0] = -32768; poll();
        require(vesim.getInputValue(1) == 65536, "failed config preserves previous bindings");
        std::filesystem::remove(path);
        require(!vesim.loadConfig(path.string()) && !vesim.hasActiveBinding(1), "missing config disables mapping");
        orbiter::platform::PublishJoystickStates({});
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
