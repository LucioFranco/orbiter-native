// Native SDL3 window/input adapter. Rendering belongs to the graphics client.
#include "SDLWindow.h"
#include "Joystick.h"
#include "Orbiter.h"
#include "Camera.h"
#include "NativeMFD.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include <stdexcept>
#include <string>
extern Camera *g_camera;
namespace orbiter::platform {
unsigned MapScancode(SDL_Scancode code) {
    switch (code) {
    case SDL_SCANCODE_ESCAPE: return OAPI_KEY_ESCAPE;
    case SDL_SCANCODE_1: return OAPI_KEY_1;
    case SDL_SCANCODE_2: return OAPI_KEY_2;
    case SDL_SCANCODE_3: return OAPI_KEY_3;
    case SDL_SCANCODE_4: return OAPI_KEY_4;
    case SDL_SCANCODE_5: return OAPI_KEY_5;
    case SDL_SCANCODE_6: return OAPI_KEY_6;
    case SDL_SCANCODE_7: return OAPI_KEY_7;
    case SDL_SCANCODE_8: return OAPI_KEY_8;
    case SDL_SCANCODE_9: return OAPI_KEY_9;
    case SDL_SCANCODE_0: return OAPI_KEY_0;
    case SDL_SCANCODE_MINUS: return OAPI_KEY_MINUS;
    case SDL_SCANCODE_EQUALS: return OAPI_KEY_EQUALS;
    case SDL_SCANCODE_BACKSPACE: return OAPI_KEY_BACK;
    case SDL_SCANCODE_TAB: return OAPI_KEY_TAB;
    case SDL_SCANCODE_Q: return OAPI_KEY_Q;
    case SDL_SCANCODE_W: return OAPI_KEY_W;
    case SDL_SCANCODE_E: return OAPI_KEY_E;
    case SDL_SCANCODE_R: return OAPI_KEY_R;
    case SDL_SCANCODE_T: return OAPI_KEY_T;
    case SDL_SCANCODE_Y: return OAPI_KEY_Y;
    case SDL_SCANCODE_U: return OAPI_KEY_U;
    case SDL_SCANCODE_I: return OAPI_KEY_I;
    case SDL_SCANCODE_O: return OAPI_KEY_O;
    case SDL_SCANCODE_P: return OAPI_KEY_P;
    case SDL_SCANCODE_LEFTBRACKET: return OAPI_KEY_LBRACKET;
    case SDL_SCANCODE_RIGHTBRACKET: return OAPI_KEY_RBRACKET;
    case SDL_SCANCODE_RETURN: return OAPI_KEY_RETURN;
    case SDL_SCANCODE_LCTRL: return OAPI_KEY_LCONTROL;
    case SDL_SCANCODE_A: return OAPI_KEY_A;
    case SDL_SCANCODE_S: return OAPI_KEY_S;
    case SDL_SCANCODE_D: return OAPI_KEY_D;
    case SDL_SCANCODE_F: return OAPI_KEY_F;
    case SDL_SCANCODE_G: return OAPI_KEY_G;
    case SDL_SCANCODE_H: return OAPI_KEY_H;
    case SDL_SCANCODE_J: return OAPI_KEY_J;
    case SDL_SCANCODE_K: return OAPI_KEY_K;
    case SDL_SCANCODE_L: return OAPI_KEY_L;
    case SDL_SCANCODE_SEMICOLON: return OAPI_KEY_SEMICOLON;
    case SDL_SCANCODE_APOSTROPHE: return OAPI_KEY_APOSTROPHE;
    case SDL_SCANCODE_GRAVE: return OAPI_KEY_GRAVE;
    case SDL_SCANCODE_LSHIFT: return OAPI_KEY_LSHIFT;
    case SDL_SCANCODE_BACKSLASH: return OAPI_KEY_BACKSLASH;
    case SDL_SCANCODE_Z: return OAPI_KEY_Z;
    case SDL_SCANCODE_X: return OAPI_KEY_X;
    case SDL_SCANCODE_C: return OAPI_KEY_C;
    case SDL_SCANCODE_V: return OAPI_KEY_V;
    case SDL_SCANCODE_B: return OAPI_KEY_B;
    case SDL_SCANCODE_N: return OAPI_KEY_N;
    case SDL_SCANCODE_M: return OAPI_KEY_M;
    case SDL_SCANCODE_COMMA: return OAPI_KEY_COMMA;
    case SDL_SCANCODE_PERIOD: return OAPI_KEY_PERIOD;
    case SDL_SCANCODE_SLASH: return OAPI_KEY_SLASH;
    case SDL_SCANCODE_RSHIFT: return OAPI_KEY_RSHIFT;
    case SDL_SCANCODE_KP_MULTIPLY: return OAPI_KEY_MULTIPLY;
    case SDL_SCANCODE_LALT: return OAPI_KEY_LALT;
    case SDL_SCANCODE_SPACE: return OAPI_KEY_SPACE;
    case SDL_SCANCODE_CAPSLOCK: return OAPI_KEY_CAPITAL;
    case SDL_SCANCODE_F1: return OAPI_KEY_F1;
    case SDL_SCANCODE_F2: return OAPI_KEY_F2;
    case SDL_SCANCODE_F3: return OAPI_KEY_F3;
    case SDL_SCANCODE_F4: return OAPI_KEY_F4;
    case SDL_SCANCODE_F5: return OAPI_KEY_F5;
    case SDL_SCANCODE_F6: return OAPI_KEY_F6;
    case SDL_SCANCODE_F7: return OAPI_KEY_F7;
    case SDL_SCANCODE_F8: return OAPI_KEY_F8;
    case SDL_SCANCODE_F9: return OAPI_KEY_F9;
    case SDL_SCANCODE_F10: return OAPI_KEY_F10;
    case SDL_SCANCODE_NUMLOCKCLEAR: return OAPI_KEY_NUMLOCK;
    case SDL_SCANCODE_SCROLLLOCK: return OAPI_KEY_SCROLL;
    case SDL_SCANCODE_KP_7: return OAPI_KEY_NUMPAD7;
    case SDL_SCANCODE_KP_8: return OAPI_KEY_NUMPAD8;
    case SDL_SCANCODE_KP_9: return OAPI_KEY_NUMPAD9;
    case SDL_SCANCODE_KP_MINUS: return OAPI_KEY_SUBTRACT;
    case SDL_SCANCODE_KP_4: return OAPI_KEY_NUMPAD4;
    case SDL_SCANCODE_KP_5: return OAPI_KEY_NUMPAD5;
    case SDL_SCANCODE_KP_6: return OAPI_KEY_NUMPAD6;
    case SDL_SCANCODE_KP_PLUS: return OAPI_KEY_ADD;
    case SDL_SCANCODE_KP_1: return OAPI_KEY_NUMPAD1;
    case SDL_SCANCODE_KP_2: return OAPI_KEY_NUMPAD2;
    case SDL_SCANCODE_KP_3: return OAPI_KEY_NUMPAD3;
    case SDL_SCANCODE_KP_0: return OAPI_KEY_NUMPAD0;
    case SDL_SCANCODE_KP_PERIOD: return OAPI_KEY_DECIMAL;
    case SDL_SCANCODE_NONUSBACKSLASH: return OAPI_KEY_OEM_102;
    case SDL_SCANCODE_F11: return OAPI_KEY_F11;
    case SDL_SCANCODE_F12: return OAPI_KEY_F12;
    case SDL_SCANCODE_KP_ENTER: return OAPI_KEY_NUMPADENTER;
    case SDL_SCANCODE_RCTRL: return OAPI_KEY_RCONTROL;
    case SDL_SCANCODE_KP_DIVIDE: return OAPI_KEY_DIVIDE;
    case SDL_SCANCODE_PRINTSCREEN: return OAPI_KEY_SYSRQ;
    case SDL_SCANCODE_RALT: return OAPI_KEY_RALT;
    case SDL_SCANCODE_PAUSE: return OAPI_KEY_PAUSE;
    case SDL_SCANCODE_HOME: return OAPI_KEY_HOME;
    case SDL_SCANCODE_UP: return OAPI_KEY_UP;
    case SDL_SCANCODE_PAGEUP: return OAPI_KEY_PRIOR;
    case SDL_SCANCODE_LEFT: return OAPI_KEY_LEFT;
    case SDL_SCANCODE_RIGHT: return OAPI_KEY_RIGHT;
    case SDL_SCANCODE_END: return OAPI_KEY_END;
    case SDL_SCANCODE_DOWN: return OAPI_KEY_DOWN;
    case SDL_SCANCODE_PAGEDOWN: return OAPI_KEY_NEXT;
    case SDL_SCANCODE_INSERT: return OAPI_KEY_INSERT;
    case SDL_SCANCODE_DELETE: return OAPI_KEY_DELETE;
    default: return 0;
    }
}
SDLWindow::SDLWindow(Orbiter &host, bool hidden, bool laptopControls) : host_(host) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) throw std::runtime_error(SDL_GetError());
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (hidden ? SDL_WINDOW_HIDDEN : 0);
#ifdef __APPLE__
    if (host_.GetGraphicsClient()) flags |= SDL_WINDOW_METAL;
#endif
    window_ = SDL_CreateWindow("Orbiter / NASSP — native port", 1280, 720, flags);
    try {
        if (!window_) throw std::runtime_error(SDL_GetError());
        if(!ImGui_ImplSDL3_InitForOther(window_))throw std::runtime_error("Cannot initialize ImGui SDL input");
        joysticks_ = std::make_unique<SDLJoysticks>();
        inputFocus_ = (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0;
        SetLaptopControls(laptopControls);
        ImGui_ImplSDL3_NewFrame();
        int w, h;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        host_.ResizeNativeViewport(w, h);
        if (auto gc = host_.GetGraphicsClient()) {
            OrbNativeWindow native{};
            native.width = w; native.height = h;
            auto props = SDL_GetWindowProperties(window_);
#ifdef __APPLE__
            metalView_ = SDL_Metal_CreateView(window_);
            if (!metalView_) throw std::runtime_error(SDL_GetError());
            native.kind = ORB_WINDOW_METAL_LAYER;
            native.window = SDL_Metal_GetLayer(metalView_);
#else
            if (SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)) {
                native.kind = ORB_WINDOW_WIN32;
                native.window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
                native.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
            } else if (SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr)) {
                native.kind = ORB_WINDOW_WAYLAND;
                native.window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
                native.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            } else {
                native.kind = ORB_WINDOW_XLIB;
                native.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
                native.id = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
                native.screen = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_SCREEN_NUMBER, 0);
            }
#endif
            if (!gc->clbkAttachNativeWindow(&native)) throw std::runtime_error("Graphics client could not attach SDL window");
        }
    } catch (...) {
        if(ImGui::GetIO().BackendPlatformUserData)ImGui_ImplSDL3_Shutdown();
        if (auto gc = host_.GetGraphicsClient()) gc->clbkDetachNativeWindow();
        if (metalView_) SDL_Metal_DestroyView(metalView_);
        if (window_) SDL_DestroyWindow(window_);
        joysticks_.reset();
        SDL_Quit();
        throw;
    }
}
SDLWindow::~SDLWindow() {
    ReleaseKeyboard();
    ImGui_ImplSDL3_Shutdown();
    if (auto gc = host_.GetGraphicsClient()) gc->clbkDetachNativeWindow();
    if (metalView_) SDL_Metal_DestroyView(metalView_);
    SDL_DestroyWindow(window_);
    joysticks_.reset();
    SDL_Quit();
}
namespace {
unsigned LaptopKey(SDL_Scancode code) {
    switch(code) {
    case SDL_SCANCODE_7:return OAPI_KEY_NUMPAD7;
    case SDL_SCANCODE_8:return OAPI_KEY_NUMPAD8;
    case SDL_SCANCODE_9:return OAPI_KEY_NUMPAD9;
    case SDL_SCANCODE_U:return OAPI_KEY_NUMPAD4;
    case SDL_SCANCODE_I:return OAPI_KEY_NUMPAD5;
    case SDL_SCANCODE_O:return OAPI_KEY_NUMPAD6;
    case SDL_SCANCODE_J:return OAPI_KEY_NUMPAD1;
    case SDL_SCANCODE_K:return OAPI_KEY_NUMPAD2;
    case SDL_SCANCODE_L:return OAPI_KEY_NUMPAD3;
    case SDL_SCANCODE_M:return OAPI_KEY_NUMPAD0;
    case SDL_SCANCODE_PERIOD:return OAPI_KEY_DECIMAL;
    case SDL_SCANCODE_SLASH:return OAPI_KEY_DIVIDE;
    case SDL_SCANCODE_LEFTBRACKET:return OAPI_KEY_SUBTRACT;
    case SDL_SCANCODE_RIGHTBRACKET:return OAPI_KEY_ADD;
    default:return MapScancode(code);
    }
}
}
void SDLWindow::ReleaseKeyboard() {
    host_.ReleaseNativeKeys();pressed_.fill(0);keyUsers_.fill(0);
}
void SDLWindow::SetLaptopControls(bool enabled) {
    // Held keys must be released and pressed again after a mode change.
    ReleaseKeyboard();laptopControls_=enabled;
    UpdateTitle();
}
void SDLWindow::UpdateTitle() {
    std::string title=host_.IsRunning()?"Orbiter / NASSP":"Orbiter / NASSP — PAUSED";
    if(auto focus=oapiGetFocusObject()) {char name[256]{};oapiGetObjectName(focus,name,sizeof name);title+=" — ";title+=name;}
    title+=laptopControls_?" — laptop numpad":" — standard keys";
    title+=" (F1: controls, F2: instruments, F4: vessel, F5: pause/resume, F6: keys)";
    if(title!=title_) {title_=title;SDL_SetWindowTitle(window_,title_.c_str());}
}
void SDLWindow::KeyEvent(SDL_Scancode code,bool down) {
    if(code<=SDL_SCANCODE_UNKNOWN || code>=SDL_SCANCODE_COUNT)return;
    auto &key=pressed_[code];
    if(down) {
        if(key)return;
        key=laptopControls_?LaptopKey(code):MapScancode(code);
        if(key && (keyUsers_[key]++==0 || !KEYDOWN(host_.KeyState(),key)))host_.NativeKeyEvent(key,true);
    } else if(key) {
        if(--keyUsers_[key]==0)host_.NativeKeyEvent(key,false);
        key=0;
    }
}
bool SDLWindow::Poll() {
    UpdateTitle();
    // Capture flags are computed by the previous ImGui frame. Release held
    // flight input even when this poll has no new OS events.
    auto &input=ImGui::GetIO();
    if(input.WantCaptureKeyboard)ReleaseKeyboard();
    if(input.WantCaptureMouse) {
        host_.ReleaseNativeCockpitMouse();
        if(g_camera) {g_camera->MouseButton(0,false);g_camera->MouseButton(1,false);}
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        auto &io=ImGui::GetIO();
        if(io.WantCaptureKeyboard)ReleaseKeyboard();
        if(io.WantCaptureMouse) {
            host_.ReleaseNativeCockpitMouse();
            if(g_camera) {g_camera->MouseButton(0,false);g_camera->MouseButton(1,false);}
        }
        if((event.type==SDL_EVENT_KEY_DOWN || event.type==SDL_EVENT_KEY_UP) && event.key.scancode==SDL_SCANCODE_F1) {
            if(event.key.down && !event.key.repeat) {
                host_.ToggleNativeUI();ReleaseKeyboard();host_.ReleaseNativeCockpitMouse();
                if(g_camera) {g_camera->MouseButton(0,false);g_camera->MouseButton(1,false);}
            }
            continue;
        }
        // WantCaptureMouse describes the previous frame. A move and click can
        // arrive together, so also hit-test the currently displayed panel.
        bool captureMouse=io.WantCaptureMouse;
        if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP)
            captureMouse|=host_.NativeUIContains(event.button.x,event.button.y);
        else if(event.type==SDL_EVENT_MOUSE_MOTION)captureMouse|=host_.NativeUIContains(event.motion.x,event.motion.y);
        else if(event.type==SDL_EVENT_MOUSE_WHEEL)captureMouse|=host_.NativeUIContains(event.wheel.mouse_x,event.wheel.mouse_y);
        if(captureMouse) {
            host_.ReleaseNativeCockpitMouse();
            if(g_camera) {g_camera->MouseButton(0,false);g_camera->MouseButton(1,false);}
            if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN)ReleaseKeyboard();
        }
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: return false;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (!event.key.repeat) {
                if(event.key.scancode==SDL_SCANCODE_F2) {if(event.key.down) {ReleaseKeyboard();host_.ReleaseNativeCockpitMouse();orbiter::NativeMFDToggle();}}
                else if(event.key.scancode==SDL_SCANCODE_F4) {if(event.key.down) {ReleaseKeyboard();host_.CycleNativeFocus();}}
                else if(event.key.scancode==SDL_SCANCODE_F5) {if(event.key.down) {ReleaseKeyboard();host_.TogglePause();}}
                else if(event.key.scancode==SDL_SCANCODE_F6) {if(event.key.down)SetLaptopControls(!laptopControls_);}
                else if(!io.WantCaptureKeyboard || !event.key.down)KeyEvent(event.key.scancode,event.key.down);
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            inputFocus_ = true; break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            inputFocus_ = false;
            SetJoystickInputEnabled(false);
            ReleaseKeyboard();
            host_.ReleaseNativeCockpitMouse();
            if (g_camera) { g_camera->MouseButton(0, false); g_camera->MouseButton(1, false); }
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            host_.ResizeNativeViewport(0,0); break;
        case SDL_EVENT_WINDOW_RESTORED: {
            int w,h; SDL_GetWindowSizeInPixels(window_,&w,&h);
            host_.ResizeNativeViewport(w,h); break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            host_.ResizeNativeViewport(event.window.data1, event.window.data2); break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if(captureMouse)break;
            if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
                int w,h;SDL_GetWindowSize(window_,&w,&h);
                const unsigned button=event.button.button==SDL_BUTTON_LEFT?0:1;
                const bool used=host_.NativeCockpitMouse(button,event.button.down,
                    event.button.x*host_.ViewW()/std::max(w,1),event.button.y*host_.ViewH()/std::max(h,1));
                if (g_camera && (!used || !event.button.down)) g_camera->MouseButton(button,event.button.down);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if(captureMouse)break;
            { int w,h;SDL_GetWindowSize(window_,&w,&h);
              host_.NativeCockpitMotion(event.motion.x*host_.ViewW()/std::max(w,1),event.motion.y*host_.ViewH()/std::max(h,1)); }
            if (g_camera) g_camera->MouseMotion(event.motion.xrel, event.motion.yrel); break;
        case SDL_EVENT_MOUSE_WHEEL:
            if(captureMouse)break;
            if (g_camera) g_camera->MouseWheel(event.wheel.y *
                (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1), KEYMOD_CONTROL(host_.KeyState()));
            break;
        }
    }
    SetJoystickInputEnabled(inputFocus_ && host_.IsRunning() && !ImGui::GetIO().WantCaptureKeyboard);
    joysticks_->Poll();
    ImGui_ImplSDL3_NewFrame();
    return true;
}
}
