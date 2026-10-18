// Wall-clock API adapter required by the original MCC worker.
#pragma once
#ifndef _WIN32
#include <thread>
#include <chrono>
#include <algorithm>
// MSVC exposed these standard algorithms through its platform headers.
using std::min;
using std::max;
inline void Sleep(unsigned milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
#endif
