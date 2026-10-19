// Wall-clock API adapter required by the original MCC worker.
#pragma once
#include <algorithm>
// Native SDK headers disable the Windows min/max macros. Preserve the
// standard algorithms used unqualified by the original NASSP sources.
using std::min;
using std::max;
#ifndef _WIN32
#include <thread>
#include <chrono>
inline void Sleep(unsigned milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
#endif
