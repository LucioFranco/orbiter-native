#pragma once
#include "OrbiterAPI.h"
#include "WgpuBridge.h"
#include <array>
#include <cmath>

// Orbiter exhaust textures contain a longitudinal flame in the left quarter
// and a nozzle flare in the upper-right quarter. Work in vessel-local doubles.
inline bool ExhaustGeometry(const EXHAUSTSPEC &spec,const VECTOR3 &eye,float alpha,
                            std::array<OrbMeshVertex,8> &vertices) {
    if(!spec.lpos || !spec.ldir || !(spec.lsize>0) || !(spec.wsize>0) ||
       !std::isfinite(spec.lsize+spec.wsize+spec.lofs) || !(alpha>0))return false;
    const double magnitude=length(*spec.ldir);
    if(!(magnitude>0) || !std::isfinite(magnitude))return false;
    const auto direction=-*spec.ldir/magnitude;
    const auto origin=*spec.lpos+direction*spec.lofs;
    auto view=origin-eye;
    if(!std::isfinite(length(view)))return false;
    view=length(view)>1e-9?unit(view):direction;
    auto side=crossp(view,direction);
    if(length(side)<1e-8)side=crossp(view,std::abs(view.y)<0.9?_V(0,1,0):_V(1,0,0));
    side=unit(side)*spec.wsize;
    const auto up=unit(crossp(view,side))*spec.wsize;
    const auto end=origin+direction*spec.lsize;
    const std::array<VECTOR3,8> points={origin+side,end+side,origin-side,end-side,
        origin-side*7+up*7,origin+side*7+up*7,origin-side*7-up*7,origin+side*7-up*7};
    constexpr float uv[8][2]={{.24f,0},{.24f,1},{.01f,0},{.01f,1},
        {.50390625f,.00390625f},{.99609375f,.00390625f},
        {.50390625f,.49609375f},{.99609375f,.49609375f}};
    for(size_t i=0;i<vertices.size();++i)vertices[i]={
        {float(points[i].x),float(points[i].y),float(points[i].z)},
        {0,0,0},{uv[i][0],uv[i][1]},{1,1,1,alpha},{1,1,1,0},{0,0,0,0}};
    return true;
}
