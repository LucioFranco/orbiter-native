#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace orbiter::platform {
// Fixed simulation ticks driven by wall time, independent of rendering cadence.
// Discard long suspension time and excessive debt instead of freezing input
// while trying to simulate a debugger pause or machine sleep.
class RealtimeSteps {
    double timestep_, pending_=0;
public:
    explicit RealtimeSteps(double timestep):timestep_(timestep) {
        if(!(timestep>0) || !std::isfinite(timestep))throw std::invalid_argument("Invalid realtime timestep");
    }
    unsigned Advance(double elapsed) {
        if(!(elapsed>=0) || !std::isfinite(elapsed))throw std::invalid_argument("Invalid elapsed time");
        pending_+=std::min(elapsed,0.25);
        const double due=std::floor((pending_+timestep_*1e-9)/timestep_);
        const auto count=static_cast<unsigned>(std::min(due,32.0));
        pending_=std::max(0.0,pending_-count*timestep_);
        if(due>32)pending_=std::fmod(pending_,timestep_);
        return count;
    }
};
}
