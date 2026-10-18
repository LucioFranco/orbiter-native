// Copyright (c) Martin Schweiger
// Licensed under the MIT License
#pragma once
#include "win32_compat.h"

// Convert CSS names or #RRGGBB into Orbiter BGR colors.
DWORD GetCSSColor(const char *col);
