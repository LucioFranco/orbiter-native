#include "Orbiter.h"
#include "Platform/SDLWindow.h"
#include "imgui.h"
#include <iostream>
#include <stdexcept>
#include <utility>
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main()
{
    try {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        Orbiter host;
        orbiter::platform::SDLWindow window(host, true);
        require(orbiter::platform::MapScancode(SDL_SCANCODE_A) == OAPI_KEY_A, "letter mapping");
        require(orbiter::platform::MapScancode(SDL_SCANCODE_KP_ENTER) == OAPI_KEY_NUMPADENTER,
                "keypad enter remains distinct from main enter");
        require(orbiter::platform::MapScancode(SDL_SCANCODE_UNKNOWN) == 0, "unknown key ignored");
        SDL_Event key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.key.scancode = SDL_SCANCODE_LSHIFT;
        key.key.down = true;
        require(SDL_PushEvent(&key), "queue shift key");
        key.key.scancode = SDL_SCANCODE_A;
        require(SDL_PushEvent(&key), "queue letter key");
        require(window.Poll(), "poll key events");
        require(KEYMOD_LSHIFT(host.KeyState()) && KEYDOWN(host.KeyState(), OAPI_KEY_A),
                "SDL events populate Orbiter DIK state");
        key.type = SDL_EVENT_KEY_UP;
        key.key.down = false;
        require(SDL_PushEvent(&key) && window.Poll(), "poll key release");
        require(!KEYDOWN(host.KeyState(), OAPI_KEY_A), "released key is cleared");
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        require(SDL_PushEvent(&focus) && window.Poll(), "poll focus loss");
        require(!KEYMOD_SHIFT(host.KeyState()), "focus loss prevents stuck modifiers");
        auto press=[&](SDL_Scancode code,bool down,bool repeat=false) {
            SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
            event.key.scancode=code;event.key.down=down;event.key.repeat=repeat;
            require(SDL_PushEvent(&event) && window.Poll(),"keyboard event dispatch");
        };
        press(SDL_SCANCODE_F6,true);press(SDL_SCANCODE_F6,false);
        require(window.LaptopControls(),"F6 enables laptop controls");
        const std::pair<SDL_Scancode,unsigned> aliases[]={
            {SDL_SCANCODE_7,OAPI_KEY_NUMPAD7},{SDL_SCANCODE_8,OAPI_KEY_NUMPAD8},{SDL_SCANCODE_9,OAPI_KEY_NUMPAD9},
            {SDL_SCANCODE_U,OAPI_KEY_NUMPAD4},{SDL_SCANCODE_I,OAPI_KEY_NUMPAD5},{SDL_SCANCODE_O,OAPI_KEY_NUMPAD6},
            {SDL_SCANCODE_J,OAPI_KEY_NUMPAD1},{SDL_SCANCODE_K,OAPI_KEY_NUMPAD2},{SDL_SCANCODE_L,OAPI_KEY_NUMPAD3},
            {SDL_SCANCODE_M,OAPI_KEY_NUMPAD0},{SDL_SCANCODE_PERIOD,OAPI_KEY_DECIMAL},{SDL_SCANCODE_SLASH,OAPI_KEY_DIVIDE},
            {SDL_SCANCODE_LEFTBRACKET,OAPI_KEY_SUBTRACT},{SDL_SCANCODE_RIGHTBRACKET,OAPI_KEY_ADD}};
        for(const auto &[code,mapped]:aliases) {
            press(code,true);require(KEYDOWN(host.KeyState(),mapped),"laptop key reaches DIK numpad state");
            require(!KEYDOWN(host.KeyState(),orbiter::platform::MapScancode(code)),"laptop alias suppresses ordinary key");
            press(code,false);require(!KEYDOWN(host.KeyState(),mapped),"laptop key releases mapped state");
        }
        press(SDL_SCANCODE_K,true);press(SDL_SCANCODE_KP_2,true);
        press(SDL_SCANCODE_K,false);
        require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"physical numpad remains held after laptop release");
        press(SDL_SCANCODE_KP_2,false);
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"last alias release clears key");
        press(SDL_SCANCODE_KP_2,true);press(SDL_SCANCODE_K,true);press(SDL_SCANCODE_KP_2,false);
        require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"laptop remains held after physical numpad release");
        press(SDL_SCANCODE_F6,true);
        require(!window.LaptopControls() && !KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"mode change releases thrust key");
        press(SDL_SCANCODE_F6,true,true);
        require(!window.LaptopControls(),"F6 repeat does not toggle mode");
        press(SDL_SCANCODE_K,true,true);
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_K),"held alias does not become ordinary key on mode change");
        press(SDL_SCANCODE_K,false);press(SDL_SCANCODE_K,true);
        require(KEYDOWN(host.KeyState(),OAPI_KEY_K),"fresh press uses standard mapping");
        window.SetLaptopControls(true);press(SDL_SCANCODE_K,true);
        require(SDL_PushEvent(&focus) && window.Poll(),"focus loss with laptop key held");
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"focus loss clears laptop thrust");
        press(SDL_SCANCODE_K,false);press(SDL_SCANCODE_K,true);
        host.ReleaseNativeKeys();press(SDL_SCANCODE_KP_2,true);
        require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"fresh alias works after host clears input");
        press(SDL_SCANCODE_K,false);press(SDL_SCANCODE_KP_2,false);
        require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"external release does not corrupt alias accounting");
        press(SDL_SCANCODE_K,true);
        ImGui::GetIO().WantCaptureKeyboard=true;
        require(window.Poll() && !KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"UI capture releases held flight key without a key-up event");
        press(SDL_SCANCODE_KP_2,true);require(!KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"UI capture suppresses new flight keys");
        press(SDL_SCANCODE_KP_2,false);press(SDL_SCANCODE_K,false);
        ImGui::GetIO().WantCaptureKeyboard=false;
        press(SDL_SCANCODE_K,true);require(KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD2),"flight input resumes after UI capture");press(SDL_SCANCODE_K,false);
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        require(SDL_PushEvent(&quit) && !window.Poll(), "quit ends event loop");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
