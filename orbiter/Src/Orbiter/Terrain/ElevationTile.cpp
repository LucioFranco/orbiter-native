#include "ElevationTile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <limits>
namespace orbiter::terrain {
namespace {
std::uint32_t U32(const std::uint8_t *p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1])<<8 | std::uint32_t(p[2])<<16 | std::uint32_t(p[3])<<24;
}
double F64(const std::uint8_t *p) {
    const std::uint64_t bits=U32(p) | (std::uint64_t(U32(p+4))<<32);
    double value; static_assert(sizeof(value)==sizeof(bits)); std::memcpy(&value,&bits,sizeof value); return value;
}
}
std::optional<ElevationTile> ElevationTile::Decode(const std::uint8_t *bytes, std::size_t size) {
    if (!bytes || size<100 || std::memcmp(bytes,"ELE\1",4)) return {};
    const auto header=U32(bytes+4), format=U32(bytes+8);
    if (header<100 || header>size || U32(bytes+12)!=stride || U32(bytes+16)!=stride ||
        U32(bytes+20)!=1 || U32(bytes+24)!=1) return {};
    unsigned width;
    if (format==0) width=0;
    else if (format==8 || format==std::uint32_t(-8)) width=1;
    else if (format==16 || format==std::uint32_t(-16)) width=2;
    else return {};
    constexpr std::size_t count=stride*stride;
    if (size-header<count*width) return {};
    const double scale=F64(bytes+28), offset=F64(bytes+36);
    // Metadata is not used for addressing: the tile's quadtree index defines it.
    for (unsigned at=28;at<100;at+=8) if (!std::isfinite(F64(bytes+at))) return {};
    ElevationTile tile; tile.heights_.resize(count);
    for (std::size_t i=0;i<count;++i) {
        int raw=0;
        if (width) {
            const auto p=bytes+header+i*width;
            raw=p[0]; if(width==2) raw|=int(p[1])<<8;
            if (format==std::uint32_t(-8) && raw>=128) raw-=256;
            if (format==std::uint32_t(-16) && raw>=32768) raw-=65536;
        }
        const double height=raw*scale+offset;
        if (!std::isfinite(height)) return {};
        tile.heights_[i]=height;
    }
    return tile;
}
bool ElevationTile::ApplyModification(const std::uint8_t *bytes, std::size_t size,
                                      double resolution, std::int16_t *destination) {
    if (!destination || !(resolution>0) || !std::isfinite(resolution)) return false;
    const auto tile=Decode(bytes,size);
    if (!tile) return false;
    const auto format=U32(bytes+8), header=U32(bytes+4);
    if (format!=0 && format!=8 && format!=std::uint32_t(-16)) return false;
    constexpr std::size_t count=stride*stride;
    std::vector<std::int16_t> result(destination,destination+count);
    for (std::size_t i=0;i<count;++i) {
        if (format==8 && bytes[header+i]==255) continue;
        if (format==std::uint32_t(-16) && bytes[header+2*i]==255 && bytes[header+2*i+1]==127) continue;
        const double value=std::trunc(tile->heights_[i]/resolution);
        if (!std::isfinite(value) || value<std::numeric_limits<std::int16_t>::min() ||
            value>std::numeric_limits<std::int16_t>::max()) return false;
        result[i]=static_cast<std::int16_t>(value);
    }
    std::copy(result.begin(),result.end(),destination);
    return true;
}
double ElevationTile::Sample(double east, double north) const {
    if (!std::isfinite(east) || !std::isfinite(north) || heights_.size()!=stride*stride)
        throw std::invalid_argument("Invalid elevation sample");
    const double x=1+std::clamp(east,-1.0/intervals,1+1.0/intervals)*intervals;
    const double y=1+std::clamp(north,-1.0/intervals,1+1.0/intervals)*intervals;
    const auto ix=std::min(unsigned(x),stride-2), iy=std::min(unsigned(y),stride-2);
    const double u=x-ix,v=y-iy;
    const auto at=iy*stride+ix;
    return (1-v)*((1-u)*heights_[at]+u*heights_[at+1])+
        v*((1-u)*heights_[at+stride]+u*heights_[at+stride+1]);
}
}
