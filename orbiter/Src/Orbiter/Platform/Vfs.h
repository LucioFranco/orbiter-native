// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace orbiter::platform {

// Orbiter assets use Windows separators and ASCII case-insensitive names.
// No lowercasing of the actual path: retain the spelling on the host filesystem.
std::filesystem::path NormalizePath(std::string_view path);

// Resolve each existing component, preserving a missing tail for output paths
// and normal stream-open failure handling. Exact matches win; ambiguous folded
// matches throw rather than selecting a different asset nondeterministically.
// Relative paths are relative to the process working directory, as in Orbiter.
// '..' is not collapsed before symlinks have been traversed.
std::filesystem::path ResolvePath(std::string_view path);

}
