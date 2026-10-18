#include "Platform/RealtimeSteps.h"
#include <iostream>
#include <stdexcept>
#include <limits>
static void require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
int main() {
    try {
        constexpr double dt=1.0/60;
        for(const unsigned fps:{20u,41u,60u,144u}) {
            orbiter::platform::RealtimeSteps clock(dt);
            unsigned steps=0;
            for(unsigned frame=0;frame<fps*5;++frame)steps+=clock.Advance(1.0/fps);
            require(steps==300,"simulation clock independent of frame rate");
        }
        orbiter::platform::RealtimeSteps jitter(dt);
        double elapsed=0;unsigned steps=0;
        for(unsigned frame=0;frame<600;++frame) {
            const double duration=(5+(frame*17)%45)/1000.0;
            elapsed+=duration;steps+=jitter.Advance(duration);
            require(std::abs(elapsed-steps*dt)<dt+1e-9,"variable rendering preserves fractional timestep debt");
        }
        orbiter::platform::RealtimeSteps paused(dt);
        require(paused.Advance(120)==15,"machine suspension is bounded");
        require(paused.Advance(0)==0,"suspension does not leave runaway debt");
        require(paused.Advance(dt)==1,"clock resumes normally after suspension");
        orbiter::platform::RealtimeSteps tiny(0.0001);
        require(tiny.Advance(0.25)==32,"per-frame work is bounded");
        require(tiny.Advance(0)<=1,"excessive tick debt is discarded");
        bool rejected=false;try {paused.Advance(std::numeric_limits<double>::quiet_NaN());}catch(const std::invalid_argument &){rejected=true;}
        require(rejected,"invalid wall time rejected");
        std::cout<<"Fixed-step realtime scheduling verified across frame rates, jitter, and suspension\n";
        return 0;
    }catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
