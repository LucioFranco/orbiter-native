// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// Linked into addon modules. Module callbacks are dispatched explicitly by
// Orbiter's module manager on all platforms, never from DllMain.
#include "OrbiterAPI.h"
#include <cstdio>
#include <cstring>

int oapiGetModuleVersion()
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    char month[4] = {};
    int day = 0, year = 0;
    if (std::sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3) return 0;
    for (int i = 0; i < 12; ++i)
        if (std::strcmp(month, months[i]) == 0)
            return (year % 100) * 10000 + (i + 1) * 100 + day;
    return 0;
}

DLLCLBK int GetModuleVersion()
{
    return oapiGetModuleVersion();
}

void dummy() {}
