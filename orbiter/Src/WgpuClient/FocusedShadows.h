#pragma once
#include "WgpuBridge.h"
#include "OrbiterAPI.h"
#include <cmath>
// Snap both cascades to the coarser texel grid. Return only the correction,
// avoiding addition/subtraction of astronomical coordinates in float precision.
// The anchor is normally the reference planet centre; sunlight axes stay global.
inline VECTOR3 ShadowFocusCorrection(const VECTOR3 &relativeFocus,const VECTOR3 &sun,
                                     double extent,const VECTOR3 &upHint=_V(0,1,0)) {
    const auto up=std::abs(dotp(upHint,sun))<0.9?upHint:
        (std::abs(sun.x)<0.9?_V(1,0,0):_V(0,0,1));
    const auto cross=crossp(up,sun);const auto right=cross/length(cross);
    const auto vertical=crossp(sun,right);
    // Two 2048px cascades, outer extent = 4 * inner extent.
    const double texel=8*extent/2048;
    const double x=dotp(right,relativeFocus),y=dotp(vertical,relativeFocus);
    return right*(-std::remainder(x,texel))+vertical*(-std::remainder(y,texel));
}
// Recover camera-relative coordinates from the engine's perspective MVP and
// project them into a local orthographic sunlight volume. Astronomical position
// subtraction has already happened in double precision before the MVP upload.
inline void FocusedShadow(OrbMeshDraw &draw,const VECTOR3 &center,const VECTOR3 &sun,
                          double extent,double fx,double fy,const VECTOR3 &upHint=_V(0,1,0)) {
    const auto up=std::abs(dotp(upHint,sun))<0.9?upHint:
        (std::abs(sun.x)<0.9?_V(1,0,0):_V(0,0,1));
    const auto cross=crossp(up,sun);const auto right=cross/length(cross);
    const auto vertical=crossp(sun,right);
    for(int col=0;col<4;++col) {
        auto p=_V(draw.mvp[col*4]/fx,draw.mvp[col*4+1]/fy,draw.mvp[col*4+3]);
        if(col==3)p-=center;
        draw.shadow_mvp[col*4]=float(dotp(right,p)/extent);
        draw.shadow_mvp[col*4+1]=float(dotp(vertical,p)/extent);
        draw.shadow_mvp[col*4+2]=float(dotp(sun,p)/(2*extent)+(col==3?0.5:0));
        draw.shadow_mvp[col*4+3]=col==3?1:0;
    }
    draw.shadow[0]=1;
    draw.shadow[1]=0.0003f;
    draw.shadow[2]=4; // outer shadow volume retains long ground shadows
}
