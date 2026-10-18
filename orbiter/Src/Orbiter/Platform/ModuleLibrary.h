// Copyright (c) Orbiter contributors
// Licensed under the MIT License
#pragma once
#include "DynamicLibrary.h"

namespace orbiter::platform {

// Addon image lifetime. InitModule/ExitModule (or their legacy opcDLL names)
// run once per loaded image, even when several vessels share that image.
// These callbacks are explicit on every OS; SDK modules must not dispatch them
// from DllMain. OpenLibrary remains available for libraries without callbacks.
HINSTANCE OpenModule(std::string_view path);
bool CloseModule(HINSTANCE handle);

}
