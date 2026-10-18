// Minimal source compatibility for the portable SDK, not a Win32 emulation layer.
#pragma once
#include <cstdint>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <strings.h>

using BYTE = std::uint8_t;
using UINT8 = std::uint8_t;
using WORD = std::uint16_t;
using INT16 = std::int16_t;
using INT32 = std::int32_t;
using INT = int;
using FLOAT = float;
using VOID = void;
using HRESULT = std::int32_t;
using LONGLONG = std::int64_t;
using DWORDLONG = std::uint64_t;
constexpr HRESULT S_OK = 0;
constexpr HRESULT E_FAIL = static_cast<HRESULT>(0x80004005u);
constexpr HRESULT E_INVALIDARG = static_cast<HRESULT>(0x80070057u);
#define FAILED(status) (static_cast<HRESULT>(status) < 0)
#define SUCCEEDED(status) (static_cast<HRESULT>(status) >= 0)
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using UINT = unsigned int;
using BOOL = int;
using COLORREF = DWORD;
using INT_PTR = std::intptr_t;
using UINT_PTR = std::uintptr_t;
using DWORD_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using WPARAM = UINT_PTR;
using LPARAM = LONG_PTR;
using LRESULT = LONG_PTR;
using HANDLE = void *;
using HINSTANCE = void *;
using HMODULE = void *;
using HWND = void *;
using HDC = void *;
using HBITMAP = void *;
using HFONT = void *;
using HPEN = void *;
using HBRUSH = void *;
using LPVOID = void *;
using LPSTR = char *;
using LPCSTR = const char *;
using LPWSTR = wchar_t *;
struct POINT { LONG x, y; };
struct SIZE { LONG cx, cy; };
using LPSIZE = SIZE *;
struct RECT { LONG left, top, right, bottom; };
using LPRECT = RECT *;
using DLGPROC = INT_PTR (*)(HWND, UINT, WPARAM, LPARAM);

#define CALLBACK
#define WINAPI
#define __cdecl
#define __int64 long long
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
constexpr WORD LOWORD(uintptr_t value) {return WORD(value & 0xffff);}
constexpr WORD HIWORD(uintptr_t value) {return WORD((value >> 16) & 0xffff);}
constexpr WPARAM MAKEWPARAM(WORD low, WORD high) {return WPARAM(low) | (WPARAM(high) << 16);}

constexpr COLORREF RGB(unsigned int r, unsigned int g, unsigned int b) {
    return (r & 255u) | ((g & 255u) << 8) | ((b & 255u) << 16);
}
constexpr BYTE GetRValue(COLORREF c) { return c & 255u; }
constexpr BYTE GetGValue(COLORREF c) { return (c >> 8) & 255u; }
constexpr BYTE GetBValue(COLORREF c) { return (c >> 16) & 255u; }
inline void ZeroMemory(void *p, std::size_t size) { std::memset(p, 0, size); }
inline int fopen_s(FILE **file, const char *path, const char *mode) {
    if (!file) return EINVAL;
    *file = nullptr;
    if (!path || !mode) return EINVAL;
    *file = std::fopen(path, mode);
    return *file ? 0 : errno;
}
inline int _fseeki64(FILE *file, std::int64_t offset, int origin) {
    static_assert(sizeof(off_t) >= sizeof(std::int64_t), "64-bit asset offsets required");
    return fseeko(file, static_cast<off_t>(offset), origin);
}
inline int _stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
inline int _strnicmp(const char *a, const char *b, std::size_t n) { return strncasecmp(a, b, n); }
inline int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
inline int strnicmp(const char *a, const char *b, std::size_t n) { return strncasecmp(a, b, n); }

// Fail closed on overflow, including the MSVC array-size overloads. Do not
// silently truncate scenario data or return success after a failed copy.
inline int strcpy_s(char *dst, std::size_t size, const char *src) {
    if (!dst || !size) return EINVAL;
    if (!src) { dst[0] = '\0'; return EINVAL; }
    if (std::strlen(src) >= size) { dst[0] = '\0'; return ERANGE; }
    std::memcpy(dst, src, std::strlen(src) + 1);
    return 0;
}
template<std::size_t N> int strcpy_s(char (&dst)[N], const char *src) {
    return strcpy_s(dst, N, src);
}
inline int vsprintf_s(char *dst, std::size_t size, const char *fmt, va_list args) {
    if (!dst || !size) return -1;
    if (!fmt) { dst[0] = '\0'; return -1; }
    int result = std::vsnprintf(dst, size, fmt, args);
    if (result < 0 || static_cast<std::size_t>(result) >= size) {
        dst[0] = '\0';
        return -1;
    }
    return result;
}
inline int sprintf_s(char *dst, std::size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vsprintf_s(dst, size, fmt, args);
    va_end(args);
    return result;
}
template<std::size_t N, typename... Args>
int sprintf_s(char (&dst)[N], const char *fmt, Args... args) {
    return sprintf_s(dst, N, fmt, args...);
}
#endif
