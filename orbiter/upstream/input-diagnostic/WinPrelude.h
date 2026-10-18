#pragma once
// Narrow standard Win32/Orbiter public type and function declarations only.
// ModuleAPI.h itself is included unchanged from the verified stock SDK.
using size_t = __SIZE_TYPE__;
using UINT = unsigned int;
using DWORD = unsigned long;
struct HINSTANCE__;
using HINSTANCE = HINSTANCE__ *;
using HANDLE = void *;
using OBJHANDLE = void *;
#define OAPIFUNC __declspec(dllimport)
#define DLLCLBK extern "C" __declspec(dllexport)
namespace oapi { class Module; }
OAPIFUNC void oapiRegisterModule(oapi::Module *); // declared below after Module forward
OAPIFUNC OBJHANDLE oapiGetFocusObject();
OAPIFUNC OBJHANDLE oapiGetVesselByName(char *name);
extern "C" {
__declspec(dllimport) HANDLE __stdcall GetProcessHeap();
__declspec(dllimport) void *__stdcall HeapAlloc(HANDLE, DWORD, size_t);
__declspec(dllimport) int __stdcall HeapFree(HANDLE, DWORD, void *);
__declspec(dllimport) DWORD __stdcall GetEnvironmentVariableA(const char *, char *, DWORD);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
__declspec(dllimport) int __stdcall WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
__declspec(dllimport) int __stdcall CloseHandle(HANDLE);
}
inline void *operator new(size_t, void *p) noexcept { return p; }
inline void operator delete(void *, void *) noexcept {}
static_assert(sizeof(void *) == 8 && sizeof(DWORD) == 4 && sizeof(UINT) == 4, "AMD64 Win32 widths");
