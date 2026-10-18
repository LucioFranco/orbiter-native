// Copyright (c) Martin Schweiger
// Licensed under the MIT License
#ifndef OAPI_IMPLEMENTATION
#define OAPI_IMPLEMENTATION
#endif
#include "CoreState.h"

// Shared by the numerical core and the application. Keep these in the library
// so modules and the thin executable cannot accidentally get separate clocks.
TimeData td;
char DBG_MSG[256] = "";
