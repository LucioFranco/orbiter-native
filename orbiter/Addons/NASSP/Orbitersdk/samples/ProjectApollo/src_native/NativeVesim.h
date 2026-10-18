/* Modified for native portability. Publication notice added 2026-10-18.
 * Original copyright, license grants and linking exceptions remain unchanged.
 * Behavioral changes are inventoried in upstream/nassp-v9/manifest.json.
 */
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Vesim {
public:
    Vesim(CbInputChanged callback, void *context);
    ~Vesim();
    Vesim(const Vesim &) = delete;
    Vesim &operator=(const Vesim &) = delete;
    bool addInput(int id, const char *name, int type,
                  int value = VESIM_DEFAULT_AXIS_VALUE, bool notify = false);
    bool addInput(VesimInputDefinition *input) {
        return input && addInput(input->id, input->name, input->type,
                                 input->defaultValue, input->notifyOnChange);
    }
    // Explicit native config; missing files leave bindings empty. Invalid files
    // throw with a line number and leave the previous bindings intact.
    bool loadConfig(const std::string &path);
    int clbkConsumeBufferedKey(DWORD, bool, char *) { return 0; }
    void poolDevices(bool enabled = true);
    void releaseControls();
    int getInputValue(int id) const;
    // True only when a uniquely selected connected device is armed and enabled.
    // Callers must preserve keyboard/manual control for unmapped inputs.
    bool hasActiveBinding(int id) const;
    void createUserConfigs() {}
private:
    struct Input { std::string name; int type, initial, value; bool notify; };
    enum class Kind { Axis, Button, Hat };
    struct Binding {
        int input, index, mask = 0;
        Kind kind;
        std::string device, serial;
        bool reverse = false, armed = false, active = false;
        double deadzone = 0;
        std::uint32_t instance = 0;
    };
    void setValue(int id, int value);
    std::map<int, Input> inputs_;
    std::vector<Binding> bindings_;
    CbInputChanged callback_;
    void *context_;
};
