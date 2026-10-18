#include "ParticleLighting.h"
#include <cmath>
#include <iostream>

int main() {
    using orbiter::graphics::ParticleOccluder;
    using orbiter::graphics::SunVisible;
    int failed=0;
    auto check=[&](bool ok,const char *description) {
        std::cout<<(ok?"PASS ":"FAIL ")<<description<<'\n';
        if(!ok)++failed;
    };
    // Original v9 Lunar Liftoff T-1m after the stock 1/60 s initialization:
    // MJD 40423.7519591566, terrain 1992.215 m below the mean lunar radius,
    // Sun elevation +22.021947 degrees. Keep astronomical coordinates to
    // exercise the actual precision and geometry, not a raised vessel origin.
    const VECTOR3 eagle=_V(74478157756.70839,-33346888.849924605,-132657786403.63461);
    const VECTOR3 moon=_V(74477067308.41176,-33328377.661850728,-132659137186.87715);
    const VECTOR3 sun=_V(672870109.7326033,-9217482.38590312,142092743.41810766);
    const double radius=1738090;
    const std::vector<ParticleOccluder> lunarBody{{moon,radius}};
    const auto radial=unit(eagle-moon),towardSun=unit(sun-eagle);
    const double altitude=length(eagle-moon)-radius;
    const double elevation=std::asin(dotp(radial,towardSun))*180/PI;
    check(altitude>-1992.22 && altitude<-1992.21 && elevation>22.02 && elevation<22.03,
          "fixture preserves actual below-datum daylight geometry");
    check(SunVisible(eagle,sun,lunarBody),"actual below-datum Eagle receives daylight");
    check(!SunVisible(eagle,eagle-(sun-eagle),lunarBody),"below-datum true night remains occluded");

    const VECTOR3 center=_V(1.5e11,0,0),low=center+_V(radius-1992.215,0,0);
    const double distance=1.5e11;
    check(SunVisible(low,low+_V(1,distance,0),{{center,radius}}),
          "below-datum Sun just above the radial horizon remains visible");
    check(!SunVisible(low,low+_V(0,distance,0),{{center,radius}}),
          "below-datum exact radial horizon is not bypassed");
    const VECTOR3 high=center+_V(radius+100,0,0);
    auto depressedSun=[&](double degrees) {
        const double angle=degrees*PI/180;
        return high+_V(-std::sin(angle),std::cos(angle),0)*distance;
    };
    check(SunVisible(high,depressedSun(0.3),{{center,radius}}),
          "elevated ray clearing the near limb remains visible");
    check(!SunVisible(high,depressedSun(0.9),{{center,radius}}),
          "elevated ray intersecting the near limb remains occluded");
    check(SunVisible(center+_V(radius+1,0,0),center+_V(distance,0,0),{{center,radius}}),
          "ordinary above-datum daylight remains visible");
    check(!SunVisible(center-_V(radius+1,0,0),center+_V(distance,0,0),{{center,radius}}),
          "ordinary above-datum night remains occluded");

    auto eclipsed=lunarBody;
    eclipsed.push_back({eagle+towardSun*1e7,1000});
    check(!SunVisible(eagle,sun,eclipsed),"another body still eclipses below-datum daylight");
    const auto sideways=unit(crossp(towardSun,radial));
    eclipsed.back().center+=sideways*1001;
    check(SunVisible(eagle,sun,eclipsed),"separate-body near miss does not eclipse daylight");
    check(SunVisible(_V(0,0,0),_V(10,0,0),{{_V(20,0,0),1}}),
          "body beyond finite light segment remains non-occluding");
    check(!SunVisible(eagle,eagle,lunarBody),"degenerate light direction remains invalid");
    return failed?1:0;
}
