// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#include "ModuleLibrary.h"
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace orbiter::platform {
namespace {
using Callback = void (*)(HINSTANCE);
struct Module {
    std::size_t references;
    Callback exit;
    bool initialized;
};
std::unordered_map<HINSTANCE, Module> modules;
// Callbacks may load another module synchronously. Serialize lifetimes while
// permitting that reentrancy; no OS loader lock is held during addon callbacks.
std::recursive_mutex moduleMutex;

Callback callback(HINSTANCE handle, const char *name, const char *legacy)
{
    auto symbol = LibrarySymbol(handle, name);
    if (!symbol) symbol = LibrarySymbol(handle, legacy);
    return reinterpret_cast<Callback>(symbol);
}
}

HINSTANCE OpenModule(std::string_view path)
{
    std::lock_guard<std::recursive_mutex> lock(moduleMutex);
    auto handle = OpenLibrary(path);
    if (!handle) return nullptr;
    auto found = modules.find(handle);
    if (found != modules.end()) {
        if (!found->second.initialized) {
            CloseLibrary(handle);
            throw std::runtime_error("Cyclic addon module initialization: " + std::string(path));
        }
        ++found->second.references;
        return handle;
    }
    auto init = callback(handle, "InitModule", "opcDLLInit");
    modules.emplace(handle, Module{1, callback(handle, "ExitModule", "opcDLLExit"), false});
    try {
        if (init) init(handle);
        modules.at(handle).initialized = true;
    } catch (...) {
        // An initialization exception is fatal to this load, but must not leave
        // the image registered as successfully initialized.
        modules.erase(handle);
        CloseLibrary(handle);
        throw;
    }
    return handle;
}

bool CloseModule(HINSTANCE handle)
{
    std::lock_guard<std::recursive_mutex> lock(moduleMutex);
    auto found = modules.find(handle);
    if (found == modules.end()) return false;
    if (!found->second.initialized) return false;
    if (--found->second.references == 0) {
        auto exit = found->second.exit;
        modules.erase(found);
        try {
            if (exit) exit(handle);
        } catch (...) {
            CloseLibrary(handle);
            throw;
        }
    }
    CloseLibrary(handle);
    return true;
}

}
