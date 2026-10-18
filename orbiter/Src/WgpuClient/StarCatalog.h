// Star catalog coordinate and colour mapping derived from Orbiter CelSphereAPI.cpp.
// Copyright (c) Martin Schweiger; licensed under the MIT License.
#pragma once
#include "GraphicsAPI.h"
#include "WgpuBridge.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <istream>
#include <stdexcept>
#include <vector>
namespace orbiter::graphics {
// Orbiter CSphere/Data/star.bin: 14-byte little-endian longitude, latitude,
// apparent magnitude, spectral index. Coordinates and colour mapping follow
// CelSphereAPI.cpp; no equatorial/ecliptic conversion is required here.
inline std::vector<OrbStar> LoadStarCatalog(std::istream &input,const StarRenderPrm &prm) {
    if(!std::isfinite(prm.mag_hi)||!std::isfinite(prm.mag_lo)||prm.mag_lo<=prm.mag_hi ||
       !std::isfinite(prm.brt_min)||prm.brt_min<=0||prm.brt_min>1)
        throw std::runtime_error("Invalid star rendering parameters");
    std::vector<OrbStar> stars;
    unsigned char bytes[14];size_t records=0;
    while(input.read(reinterpret_cast<char *>(bytes),sizeof(bytes)) || input.gcount()) {
        if(input.gcount()!=sizeof(bytes))throw std::runtime_error("Truncated star catalog record");
        if(++records>2000000)throw std::runtime_error("Star catalog too large");
        auto number=[&](int offset) {
            uint32_t bits=uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|
                (uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);
            float value;std::memcpy(&value,&bits,4);return double(value);
        };
        const double lng=number(0),lat=number(4),mag=number(8);
        if(!std::isfinite(lng)||!std::isfinite(lat)||!std::isfinite(mag)||std::abs(lat)>PI/2+1e-6)
            throw std::runtime_error("Invalid star catalog coordinates");
        if(mag>=prm.mag_lo)continue;
        const double fraction=(mag-prm.mag_hi)/(prm.mag_lo-prm.mag_hi);
        const double brightness=std::clamp(prm.map_log?std::pow(prm.brt_min,fraction):1+(prm.brt_min-1)*fraction,prm.brt_min,1.0);
        const double spec=std::min(unsigned(bytes[12])|(unsigned(bytes[13])<<8),70u);
        double color[]={spec<25?0.75+spec/25*0.25:1, spec<20?0.85+spec/20*0.15:spec<50?1:0.75+(70-spec)/20*0.25,
            spec<30?1:0.6+(70-spec)/40*0.4};
        const double scale=std::min(3/(color[0]+color[1]+color[2]),1/(brightness*std::max({color[0],color[1],color[2]})));
        OrbStar star{{float(std::cos(lat)*std::cos(lng)),float(std::sin(lat)),float(std::cos(lat)*std::sin(lng))},
            {float(color[0]*scale*brightness),float(color[1]*scale*brightness),float(color[2]*scale*brightness)},float(1+brightness)};
        stars.push_back(star);
    }
    if(input.bad())throw std::runtime_error("Cannot read star catalog");
    return stars;
}
}
