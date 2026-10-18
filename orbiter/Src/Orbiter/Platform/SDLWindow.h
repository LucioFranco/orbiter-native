#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include "SDLJoystick.h"
#include <string>
class Orbiter;
namespace orbiter::platform {
unsigned MapScancode(SDL_Scancode code);
class SDLWindow {
public:
    explicit SDLWindow(Orbiter &host, bool hidden = false, bool laptopControls = false);
    ~SDLWindow();
    SDLWindow(const SDLWindow &) = delete;
    SDLWindow &operator=(const SDLWindow &) = delete;
    bool Poll();
    void SetLaptopControls(bool enabled);
    bool LaptopControls() const {return laptopControls_;}
    SDL_Window *Handle() const { return window_; }
private:
    void KeyEvent(SDL_Scancode code,bool down);
    void ReleaseKeyboard();
    void UpdateTitle();
    std::string title_;
    bool laptopControls_=false;
    bool inputFocus_=false;
    std::array<unsigned,SDL_SCANCODE_COUNT> pressed_{};
    std::array<unsigned,256> keyUsers_{};
    Orbiter &host_;
    std::unique_ptr<SDLJoysticks> joysticks_;
    SDL_Window *window_ = nullptr;
    SDL_MetalView metalView_ = nullptr;
};
}
