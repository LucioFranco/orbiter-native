/* Modified for native portability. Publication notice added 2026-10-18.
 * Original copyright, license grants and linking exceptions remain unchanged.
 * Behavioral changes are inventoried in upstream/nassp-v9/manifest.json.
 */
#include "Orbitersdk.h"
#include "../src_aux/vesim.h"
#include "Platform/Joystick.h"
#include "Platform/Vfs.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

Vesim::Vesim(CbInputChanged callback, void *context) : callback_(callback), context_(context) {
    orbiter::platform::RegisterJoystickRelease(this, [](void *owner) { static_cast<Vesim *>(owner)->releaseControls(); });
}
Vesim::~Vesim() { orbiter::platform::UnregisterJoystickRelease(this); }

bool Vesim::addInput(int id, const char *name, int type, int value, bool notify) {
    if (id < 0 || id >= VESIM_MAX_INPUTS || !name ||
        (type != VESIM_INPUTTYPE_BUTTON && type != VESIM_INPUTTYPE_AXIS)) return false;
    for (const auto &[key, input] : inputs_) if (input.name == name) return false;
    return inputs_.emplace(id, Input{name, type, value, value, notify}).second;
}
bool Vesim::loadConfig(const std::string &path) {
    std::ifstream file(orbiter::platform::ResolvePath(path));
    if (!file.is_open()) { releaseControls(); bindings_.clear(); return false; }
    std::vector<Binding> parsed;
    std::string line;
    unsigned number = 0;
    while (std::getline(file, line)) {
        ++number;
        std::istringstream row(line);
        row >> std::ws;
        if (row.eof() || row.peek() == '#') continue;
        auto fail = [&]() { throw std::runtime_error(path + ":" + std::to_string(number) + ": invalid joystick binding"); };
        Binding binding{};
        std::string name, kind;
        if (!(row >> std::quoted(name) >> std::quoted(binding.device) >> std::quoted(binding.serial) >> kind >> binding.index)) fail();
        if (binding.device.empty() || binding.index < 0 || binding.index > 255) fail();
        auto input = std::find_if(inputs_.begin(), inputs_.end(), [&](const auto &item) { return item.second.name == name; });
        if (input == inputs_.end()) fail();
        binding.input = input->first;
        if (std::any_of(parsed.begin(), parsed.end(), [&](const Binding &other) { return other.input == binding.input; })) fail();
        if (kind == "Axis") {
            binding.kind = Kind::Axis;
            int reverse;
            if (input->second.type != VESIM_INPUTTYPE_AXIS || !(row >> reverse >> binding.deadzone) ||
                (reverse != 0 && reverse != 1) || !std::isfinite(binding.deadzone) ||
                binding.deadzone < 0 || binding.deadzone >= 1) fail();
            binding.reverse = reverse != 0;
        } else if (kind == "Button") {
            binding.kind = Kind::Button;
            if (input->second.type != VESIM_INPUTTYPE_BUTTON) fail();
        } else if (kind == "Hat") {
            binding.kind = Kind::Hat;
            if (input->second.type != VESIM_INPUTTYPE_BUTTON || !(row >> binding.mask) ||
                binding.mask < 1 || binding.mask > 15 || (binding.mask & 5) == 5 || (binding.mask & 10) == 10) fail();
        } else fail();
        row >> std::ws;
        if (!row.eof() && row.peek() != '#') fail();
        parsed.push_back(std::move(binding));
    }
    if (file.bad()) throw std::runtime_error("Cannot read joystick config: " + path);
    releaseControls();
    bindings_ = std::move(parsed);
    return !bindings_.empty();
}
void Vesim::setValue(int id, int value) {
    auto &input = inputs_.at(id);
    if (input.value == value) return;
    input.value = value;
    if (input.notify && callback_) callback_(id, input.type == VESIM_INPUTTYPE_AXIS ?
        VESIM_EVTTYPE_AXIS_CHANGED : (value ? VESIM_EVTTYPE_BUTTON_ON : VESIM_EVTTYPE_BUTTON_OFF), value, context_);
}
void Vesim::releaseControls() {
    for (auto &binding : bindings_) { binding.armed = binding.active = false; binding.instance = 0; }
    for (const auto &[id, input] : inputs_) setValue(id, input.initial);
}
void Vesim::poolDevices(bool enabled) {
    if (!enabled || !orbiter::platform::JoystickInputEnabled()) { releaseControls(); return; }
    for (auto &binding : bindings_) {
        const orbiter::platform::JoystickState *selected = nullptr;
        unsigned matches = 0;
        for (const auto &device : orbiter::platform::JoystickStates()) {
            if ((binding.device == "*" || binding.device == device.guid || binding.device == device.name) &&
                (binding.serial.empty() || binding.serial == device.serial)) { selected = &device; ++matches; }
        }
        auto reset = [&]() {
            binding.armed = binding.active = false; binding.instance = 0;
            setValue(binding.input, inputs_.at(binding.input).initial);
        };
        // Refuse ambiguous devices instead of silently changing which stick flies.
        if (matches != 1) { reset(); continue; }
        if (binding.instance != selected->instance) { reset(); binding.instance = selected->instance; }
        int value = 0;
        bool neutral = false;
        const auto index = static_cast<std::size_t>(binding.index);
        if (binding.kind == Kind::Axis) {
            if (index >= selected->axes.size()) { reset(); continue; }
            const int raw = selected->axes[index];
            double axis = raw < 0 ? raw / 32768.0 : raw / 32767.0;
            neutral = std::abs(axis) <= std::max(binding.deadzone, 0.02);
            axis = std::abs(axis) <= binding.deadzone ? 0 :
                std::copysign((std::abs(axis) - binding.deadzone) / (1 - binding.deadzone), axis);
            if (binding.reverse) axis = -axis;
            value = static_cast<int>(std::lround((axis + 1) * 32768));
        } else if (binding.kind == Kind::Button) {
            if (index >= selected->buttons.size()) { reset(); continue; }
            value = selected->buttons[index] != 0; neutral = value == 0;
        } else {
            if (index >= selected->hats.size()) { reset(); continue; }
            value = (selected->hats[index] & binding.mask) == binding.mask; neutral = selected->hats[index] == 0;
        }
        if (!binding.armed && neutral) binding.armed = true;
        binding.active = binding.armed;
        setValue(binding.input, binding.active ? value : inputs_.at(binding.input).initial);
    }
}
int Vesim::getInputValue(int id) const {
    const auto input = inputs_.find(id);
    return input == inputs_.end() ? VESIM_DEFAULT_AXIS_VALUE : input->second.value;
}
bool Vesim::hasActiveBinding(int id) const {
    return std::any_of(bindings_.begin(), bindings_.end(), [id](const Binding &binding) {
        return binding.input == id && binding.active;
    });
}
