#include "Orbitersdk.h"
#include "GraphicsAPI.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

static_assert(sizeof(DWORD) == 4 && sizeof(LONG) == 4);
static_assert(sizeof(RECT) == 16 && sizeof(POINT) == 8);
static_assert(sizeof(oapi::IVECTOR2) == sizeof(POINT));
static_assert(sizeof(oapi::FVECTOR4) == 16 && alignof(oapi::FVECTOR4) == 16);
static_assert(sizeof(oapi::FMATRIX4) == 64 && alignof(oapi::FMATRIX4) == 16);
static_assert(sizeof(WPARAM) == sizeof(void *) && sizeof(LPARAM) == sizeof(void *));
static_assert(sizeof(OAPI_MSGTYPE) == sizeof(void *));

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *name) {
        if (!ok) { std::fprintf(stderr, "FAILED: %s\n", name); ++failures; }
    };
    check(RGB(0x12, 0x34, 0x56) == 0x563412, "RGB byte order");
    check(GetRValue(RGB(1, 2, 3)) == 1 && GetGValue(RGB(1, 2, 3)) == 2 &&
          GetBValue(RGB(1, 2, 3)) == 3, "RGB channel extraction");
    check(_stricmp("Apollo", "APOLLO") == 0, "case insensitive comparison");
    char buffer[8];
    check(strcpy_s(buffer, "Apollo") == 0 && std::strcmp(buffer, "Apollo") == 0, "array copy");
    check(sprintf_s(buffer, "%s%d", "LM", 5) == 3 && std::strcmp(buffer, "LM5") == 0, "array format");
#ifndef _WIN32 // MSVC invokes its invalid-parameter handler for these inputs.
    check(strcpy_s(buffer, "too long for buffer") == ERANGE && buffer[0] == '\0', "copy overflow");
    check(sprintf_s(buffer, sizeof buffer, "%s", "too long for buffer") < 0 && buffer[0] == '\0', "format overflow");
    check(strcpy_s(buffer, nullptr) == EINVAL && buffer[0] == '\0', "null source");
    // Tile archives can exceed 4 GiB. Seeking must retain the full offset on
    // LP64 systems; no large allocation or file write is needed for this check.
    FILE *largeFile = std::tmpfile();
    check(largeFile != nullptr, "temporary file for large seek");
    if (largeFile) {
        constexpr std::int64_t offset = (std::int64_t{1} << 32) + 17;
        check(_fseeki64(largeFile, offset, SEEK_SET) == 0 && ftello(largeFile) == offset,
              "64-bit tile archive seek");
        std::fclose(largeFile);
    }
#endif
    return failures ? 1 : 0;
}
