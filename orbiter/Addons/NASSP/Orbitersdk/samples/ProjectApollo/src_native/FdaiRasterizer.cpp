/* Modified for native portability. Publication notice added 2026-10-18.
 * Original copyright, license grants and linking exceptions remain unchanged.
 * Behavioral changes are inventoried in upstream/nassp-v9/manifest.json.
 */
#include "FdaiRasterizer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace nassp {
FdaiRasterizer::Pixels FdaiRasterizer::Render(const orbiter::platform::ImageRGBA &texture,double roll,double yaw,double pitch) {
    if(!texture.width || !texture.height || texture.pixels.size()!=std::size_t(texture.width)*texture.height*4)
        throw std::invalid_argument("Invalid FDAI ball texture");
    if(!std::isfinite(roll) || !std::isfinite(yaw) || !std::isfinite(pitch))throw std::invalid_argument("Invalid FDAI attitude");
    constexpr double pi=3.14159265358979323846;
    // Same eye (0,-35,0), 45 degree field of view and radius 12 as the GLU ball.
    const double tangent=std::tan(pi/8),cr=std::cos(-pi/2-roll),sr=std::sin(-pi/2-roll);
    const double cy=std::cos(-yaw),sy=std::sin(-yaw),cp=std::cos(-pitch),sp=std::sin(-pitch);
    Pixels pixels{};
    for(unsigned py=0;py<size;++py)for(unsigned px=0;px<size;++px) {
        auto *out=&pixels[(py*size+px)*4];out[3]=255;
        double dx=(2*(px+.5)/size-1)*tangent,dz=(1-2*(py+.5)/size)*tangent;
        const double a=dx*dx+1+dz*dz,disc=35*35-a*(35*35-12*12);
        if(disc<0)continue;
        const double distance=(35-std::sqrt(disc))/a;
        const double x=dx*distance,y=-35+distance,z=dz*distance;
        // Inverse of Ry(90deg) Ry(roll) Rx(yaw) Rz(pitch).
        const double x1=cr*x+sr*z,z1=-sr*x+cr*z;
        const double y2=cy*y-sy*z1,z2=sy*y+cy*z1;
        const double x3=cp*x1-sp*y2,y3=sp*x1+cp*y2;
        double u=std::atan2(x3,y3)/(2*pi);u-=std::floor(u);
        const double v=std::acos(std::clamp(z2/12.0,-1.0,1.0))/pi;
        // GLU s advances from +Y toward +X; t runs south to north.
        // https://xorg.freedesktop.org/archive/X11R6.9.0/doc/html/gluSphere.3.html
        // Repeat the longitude seam and clamp latitude at either pole.
        const double tx=u*texture.width-.5,ty=std::clamp(v*texture.height-.5,0.0,double(texture.height-1));
        const int ix=int(std::floor(tx)),iy=int(std::floor(ty));
        const double fx=tx-std::floor(tx),fy=ty-iy;
        auto sample=[&](int sx,int sy,unsigned channel) {
            sx=(sx%int(texture.width)+int(texture.width))%int(texture.width);
            sy=std::clamp(sy,0,int(texture.height)-1);
            return texture.pixels[(std::size_t(sy)*texture.width+sx)*4+channel];
        };
        for(unsigned c=0;c<3;++c)out[c]=static_cast<unsigned char>(std::lround(
            (sample(ix,iy,c)*(1-fx)+sample(ix+1,iy,c)*fx)*(1-fy)+
            (sample(ix,iy+1,c)*(1-fx)+sample(ix+1,iy+1,c)*fx)*fy));
    }
    return pixels;
}
}
