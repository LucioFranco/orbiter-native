// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#include "Vfs.h"
#include <algorithm>
#include <stdexcept>

namespace orbiter::platform {
namespace fs = std::filesystem;

static std::string FoldAscii(std::string value)
{
    for (char &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}

fs::path NormalizePath(std::string_view path)
{
    std::string normalized(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return fs::u8path(normalized);
}

fs::path ResolvePath(std::string_view path)
{
    if (path.empty()) return {};
    fs::path input = fs::absolute(NormalizePath(path));
    fs::path resolved = input.root_path();
    for (const auto &component : input.relative_path()) {
        if (component == ".") continue;
        auto candidate = resolved / component;
        // Preserve dot-dot traversal semantics, especially across symlinks.
        if (component == "..") {
            resolved = candidate;
            continue;
        }
        if (!fs::is_directory(resolved)) {
            resolved = candidate;
            continue;
        }
        const auto folded = FoldAscii(component.u8string());
        fs::path match;
        bool ambiguous = false;
        for (const auto &entry : fs::directory_iterator(resolved)) {
            if (entry.path().filename() == component) {
                match = entry.path();
                ambiguous = false;
                break;
            }
            if (FoldAscii(entry.path().filename().u8string()) != folded) continue;
            if (!match.empty()) ambiguous = true;
            match = entry.path();
        }
        if (ambiguous) throw std::runtime_error("Ambiguous asset path: " + candidate.u8string());
        resolved = match.empty() ? candidate : match;
    }
    return resolved;
}

}
