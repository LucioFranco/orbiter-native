#pragma once
#include "OrbiterAPI.h"
#include <algorithm>
#include <vector>
namespace orbiter::graphics {
struct ParticleOccluder {VECTOR3 center;double radius;};
// Test the finite segment to the Sun in double precision. Subtract the
// particle origin before projection to preserve metre-scale lunar positions.
inline bool SunVisible(const VECTOR3 &position,
                                const VECTOR3 &sun,const std::vector<ParticleOccluder> &bodies) {
    const auto ray=sun-position;const double distance=length(ray);
    if(!(distance>0) || !std::isfinite(distance))return false;
    const auto direction=ray/distance;
    for(const auto &body:bodies) {
        if(!(body.radius>0))continue;
        const auto offset=body.center-position;
        const double projection=dotp(offset,direction);
        // Match D3D9 SunLightColor's sunlit-side handling (ca > 0). Terrain
        // can place the observer below a body's mean-radius sphere: a body
        // behind an above-horizon ray must not block it at the ray origin.
        if(projection<0)continue;
        const double along=std::min(projection,distance);
        const auto separation=offset-direction*along;
        if(dotp(separation,separation)<body.radius*body.radius)return false;
    }
    return true;
}
inline float ParticleBrightness(bool emissive,const VECTOR3 &position,
                                const VECTOR3 &sun,const std::vector<ParticleOccluder> &bodies) {
    if(emissive)return 1.0f;
    // Isotropic diffuse sprite approximation; emissive streams bypass lighting.
    return SunVisible(position,sun,bodies)?0.6f:0.03f;
}
}
