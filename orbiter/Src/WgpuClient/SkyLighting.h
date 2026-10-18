#pragma once
#include "OrbiterAPI.h"
#include <algorithm>
#include <cmath>
namespace orbiter::graphics {
// Approximate integrated diffuse Earth skylight for exterior materials. The
// existing scalar material model cannot yet represent a directional sky dome.
// Fade with atmospheric density and solar elevation; airless worlds and orbital
// night retain the renderer's small baseline rather than ground daylight fill.
inline float EarthSkyAmbient(const VECTOR3 &position,const VECTOR3 &earth,
                             double radius,const VECTOR3 &sunDirection) {
    const auto relative=position-earth;const double distance=length(relative);
    if(!(radius>0) || !(distance>0))return 0.03f;
    const double altitude=std::max(0.0,distance-radius);
    const double elevation=dotp(relative/distance,sunDirection);
    const double daylight=std::clamp((elevation+0.1)/0.4,0.0,1.0);
    const double smooth=daylight*daylight*(3-2*daylight);
    return float(0.03+0.22*smooth*std::exp(-altitude/8000.0));
}
}
