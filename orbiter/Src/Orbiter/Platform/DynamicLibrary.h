// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#pragma once
#include "win32_compat.h"
#include <filesystem>
#include <string_view>

namespace orbiter::platform {

const char *ModuleSuffix();
std::filesystem::path ResolveModulePath(std::string_view path);
HINSTANCE OpenLibrary(std::string_view path);
void CloseLibrary(HINSTANCE handle) noexcept;
void *LibrarySymbol(HINSTANCE handle, const char *name);
const std::string &LibraryError();

// Low-level image ownership only. Addon InitModule/ExitModule dispatch belongs
// to the engine's module manager, not to OS loader initialization callbacks.
class DynamicLibrary {
public:
    explicit DynamicLibrary(std::string_view path);
    ~DynamicLibrary();
    DynamicLibrary(const DynamicLibrary &) = delete;
    DynamicLibrary &operator=(const DynamicLibrary &) = delete;
    DynamicLibrary(DynamicLibrary &&other) noexcept;
    DynamicLibrary &operator=(DynamicLibrary &&other) noexcept;

    // Optional callbacks return nullptr; a missing mandatory callback is the
    // caller's error. Never query the process-global symbol table on a null handle.
    void *FindSymbol(const char *name) const;
    HINSTANCE Handle() const { return handle_; }
    const std::filesystem::path &Path() const { return path_; }

private:
    void Close() noexcept;
    HINSTANCE handle_ = nullptr;
    std::filesystem::path path_;
};

}
