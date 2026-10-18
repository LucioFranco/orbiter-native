// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#include "DynamicLibrary.h"
#include "Vfs.h"
#include <stdexcept>
#include <system_error>
#include <utility>
#include <algorithm>
#ifndef _WIN32
#include <dlfcn.h>
#endif

namespace orbiter::platform {

const char *ModuleSuffix()
{
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

static thread_local std::string loaderError;
const std::string &LibraryError() { return loaderError; }

std::filesystem::path ResolveModulePath(std::string_view path)
{
    auto native = NormalizePath(path);
    // Configuration files retain their historic .dll module spelling. Only
    // the filename changes; no legacy binary is loaded on another platform.
    auto extension = native.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch);
    });
    if (extension == ".dll")
        native.replace_extension(ModuleSuffix());
    return ResolvePath(native.string());
}

HINSTANCE OpenLibrary(std::string_view path)
{
    loaderError.clear();
    try {
        auto resolved = ResolveModulePath(path);
        if (resolved.empty()) throw std::invalid_argument("Empty module path");
#ifdef _WIN32
        auto handle = LoadLibraryExW(resolved.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!handle) loaderError = std::system_error(GetLastError(), std::system_category(), resolved.string()).what();
#else
        auto handle = dlopen(resolved.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            const char *error = dlerror();
            loaderError = "Loading " + resolved.string() + ": " + (error ? error : "unknown loader error");
        }
#endif
        return handle;
    } catch (const std::exception &error) {
        loaderError = error.what();
        return nullptr;
    }
}

void CloseLibrary(HINSTANCE handle) noexcept
{
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
}

void *LibrarySymbol(HINSTANCE handle, const char *name)
{
    if (!handle || !name) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void *>(GetProcAddress(handle, name));
#else
    return dlsym(handle, name);
#endif
}

DynamicLibrary::DynamicLibrary(std::string_view path) : path_(ResolveModulePath(path))
{
    handle_ = OpenLibrary(path_.string());
    if (!handle_) throw std::runtime_error(LibraryError());
}

DynamicLibrary::~DynamicLibrary() { Close(); }

DynamicLibrary::DynamicLibrary(DynamicLibrary &&other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), path_(std::move(other.path_)) {}

DynamicLibrary &DynamicLibrary::operator=(DynamicLibrary &&other) noexcept
{
    if (this != &other) {
        Close();
        handle_ = std::exchange(other.handle_, nullptr);
        path_ = std::move(other.path_);
    }
    return *this;
}

void DynamicLibrary::Close() noexcept
{
    CloseLibrary(handle_);
    handle_ = nullptr;
}

void *DynamicLibrary::FindSymbol(const char *name) const
{
    return LibrarySymbol(handle_, name);
}

}
