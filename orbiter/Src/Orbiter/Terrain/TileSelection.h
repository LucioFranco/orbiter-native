#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <vector>
namespace orbiter::terrain {
struct TileAddress {
    unsigned level, latitude, longitude;
};
// Conservative horizon rejection for terrain contained between two concentric
// spheres. The inner sphere must be below all terrain, the outer above it.
// Keep cameras inside the inner sphere and large root tiles unconditionally.
inline bool TerrainTileAboveHorizon(const TileAddress &tile,const std::array<double,3> &eye,
                                   double radius,double reliefBound) {
    constexpr double pi=3.14159265358979323846;
    const double inner=radius-reliefBound,outer=radius+reliefBound;
    const double distance=std::hypot(eye[0],eye[1],eye[2]);
    if(!(inner>0) || !(distance>inner) || reliefBound<0)return true;
    const double span=std::ldexp(pi,4-int(tile.level));
    const double lat=pi/2-(tile.latitude+0.5)*span,lon=-pi+(tile.longitude+0.5)*span;
    const double cosine=std::clamp((eye[0]*std::cos(lat)*std::cos(lon)+eye[1]*std::sin(lat)+eye[2]*std::cos(lat)*std::sin(lon))/distance,-1.0,1.0);
    // Travel from the tile centre along a meridian then a latitude circle:
    // each half-span bounds one leg, so span bounds the entire angular radius.
    const double visibleAngle=std::acos(inner/distance)+std::acos(inner/outer);
    return std::acos(cosine)<=visibleAngle+span+1e-10;
}
// Select a complete, non-overlapping cover. Retaining the far side makes the
// first renderer integration independent of horizon/frustum culling accuracy.
// Highest projected grid spacing is refined first within a strict leaf budget.
inline std::vector<TileAddress> SelectTerrainTiles(const std::array<double,3> &eye,
    double radius,double focalPixels,double targetPixels=16,unsigned maxLevel=18,
    unsigned maxTiles=512,unsigned segments=32) {
    if(!(radius>0) || !std::isfinite(radius) || !(focalPixels>0) || !std::isfinite(focalPixels) ||
       !(targetPixels>0) || !std::isfinite(targetPixels) || maxLevel<4 || maxLevel>30 || maxTiles<2 ||
       !segments || segments>256 || 256%segments ||
       !std::all_of(eye.begin(),eye.end(),[](double x){return std::isfinite(x);}))
        throw std::invalid_argument("Invalid terrain selection request");
    struct Candidate {TileAddress tile;double priority;std::uint64_t order;};
    struct Compare {bool operator()(const Candidate &a,const Candidate &b) const {
        return a.priority==b.priority?a.order>b.order:a.priority<b.priority;
    }};
    std::priority_queue<Candidate,std::vector<Candidate>,Compare> pending;
    std::uint64_t order=0;
    auto enqueue=[&](TileAddress tile) {
        constexpr double pi=3.14159265358979323846;
        const double span=std::ldexp(pi,4-int(tile.level));
        const double lat=pi/2-(tile.latitude+0.5)*span,lon=-pi+(tile.longitude+0.5)*span;
        const std::array<double,3> center={radius*std::cos(lat)*std::cos(lon),radius*std::sin(lat),radius*std::cos(lat)*std::sin(lon)};
        const double distance=std::hypot(eye[0]-center[0],eye[1]-center[1],eye[2]-center[2]);
        // A conservative bounding sphere of the spherical rectangle. A one-metre
        // distance floor also bounds selection for cameras on/below the surface.
        const double nearest=std::max(1.0,distance-2*radius*std::sin(std::min(pi,span)/2));
        const double pixels=radius*span*focalPixels/(segments*nearest);
        pending.push({tile,tile.level<maxLevel?pixels:0,order++});
    };
    enqueue({4,0,0});enqueue({4,0,1});
    while(pending.size()+3<=maxTiles && pending.top().priority>targetPixels) {
        const auto parent=pending.top().tile;pending.pop();
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
            enqueue({parent.level+1,parent.latitude*2+y,parent.longitude*2+x});
    }
    std::vector<TileAddress> result;result.reserve(pending.size());
    while(!pending.empty()){result.push_back(pending.top().tile);pending.pop();}
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){
        if(a.level!=b.level)return a.level<b.level;
        if(a.latitude!=b.latitude)return a.latitude<b.latitude;
        return a.longitude<b.longitude;
    });
    return result;
}
}
