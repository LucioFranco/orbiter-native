#pragma once
#include <stdint.h>
// Native handles supplied by SDL. This describes window-system integration,
// not a renderer backend. All systems use the same wgpu pipelines.
typedef enum OrbWindowKind {
    ORB_WINDOW_METAL_LAYER = 1,
    ORB_WINDOW_WIN32 = 2,
    ORB_WINDOW_XLIB = 3,
    ORB_WINDOW_WAYLAND = 4
} OrbWindowKind;
typedef struct OrbNativeWindow {
    uint32_t kind, width, height;
    int32_t screen;
    void *window;  // CAMetalLayer / HWND / wl_surface (unused for Xlib)
    void *display; // HINSTANCE / Display / wl_display
    uint64_t id;   // Xlib Window
} OrbNativeWindow;
