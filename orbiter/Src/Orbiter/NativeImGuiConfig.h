#pragma once
#ifdef _WIN32
#ifdef IMGUI_BUILD_CORE
#define IMGUI_API __declspec(dllexport)
#else
#define IMGUI_API __declspec(dllimport)
#endif
#endif
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
