#pragma once
#include "ElevationTile.h"
#include <array>
#include <functional>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace orbiter::terrain {
struct TerrainVertex {
    std::array<float,3> position, normal;
    std::array<float,2> uv;
};
struct TerrainGeometry {
    // Body-local origin stays double precision. Subtract the camera in double
    // precision before uploading a transform for these local float vertices.
    std::array<double,3> origin;
    std::vector<TerrainVertex> vertices;
    std::vector<std::uint32_t> indices;
};
// Orbiter level 4 has one latitude band and two longitude hemispheres.
// Latitude tile indices increase north-to-south; ELE rows run south-to-north.
inline TerrainGeometry BuildTileGeometry(unsigned level,unsigned latitude,unsigned longitude,
                                         double radius,const ElevationTile *elevation,
                                         unsigned segments=32,double skirtDepth=0,
                                         const std::function<double(double,double)> &heightAt={}) {
    if(level<4 || level>30 || !(radius>0) || !std::isfinite(radius) ||
       !segments || segments>256 || 256%segments || !std::isfinite(skirtDepth) || skirtDepth<0 || skirtDepth>=radius) throw std::invalid_argument("Invalid terrain geometry request");
    const std::uint32_t bands=std::uint32_t(1)<<(level-4);
    if(latitude>=bands || longitude>=2*bands) throw std::invalid_argument("Invalid terrain tile coordinate");
    constexpr double pi=3.14159265358979323846;
    const double span=pi/bands;
    auto point=[&](double u,double v) {
        // Evaluate shared coordinates from the same global grid expression.
        // Adding a local offset to independently rounded tile origins can place
        // identical edges on opposite sides of an elevation-cache boundary.
        const double lat=pi/2-span*(double(latitude)+1-v);
        const double lon=-pi+span*(double(longitude)+u);
        const double r=radius+(heightAt?heightAt(lon,lat):(elevation?elevation->Sample(u,v):0));
        return std::array<double,3>{r*std::cos(lat)*std::cos(lon),r*std::sin(lat),r*std::cos(lat)*std::sin(lon)};
    };
    TerrainGeometry out;out.origin=point(0.5,0.5);
    out.vertices.reserve((segments+1)*(segments+1));
    for(unsigned y=0;y<=segments;++y)for(unsigned x=0;x<=segments;++x) {
        const double u=double(x)/segments,v=double(y)/segments;
        const auto p=point(u,v);
        // The padded elevation samples allow central differences at tile edges.
        constexpr double step=1.0/256;
        const auto w=point(u-step,v),e=point(u+step,v),s=point(u,v-step),n=point(u,v+step);
        std::array<double,3> du{},dv{},normal{};
        for(unsigned k=0;k<3;++k){du[k]=e[k]-w[k];dv[k]=n[k]-s[k];}
        normal={dv[1]*du[2]-dv[2]*du[1],dv[2]*du[0]-dv[0]*du[2],dv[0]*du[1]-dv[1]*du[0]};
        double length=std::hypot(normal[0],normal[1],normal[2]);
        if(length<1e-12) {normal=p;length=std::hypot(p[0],p[1],p[2]);}
        if(normal[0]*p[0]+normal[1]*p[1]+normal[2]*p[2]<0)for(auto &value:normal)value=-value;
        TerrainVertex vertex{};
        for(unsigned k=0;k<3;++k){vertex.position[k]=float(p[k]-out.origin[k]);vertex.normal[k]=float(normal[k]/length);}
        vertex.uv={float(u),float(1-v)};out.vertices.push_back(vertex);
    }
    for(unsigned y=0;y<segments;++y)for(unsigned x=0;x<segments;++x) {
        const auto a=y*(segments+1)+x,b=a+segments+1;
        for(auto index:{a,b,a+1,a+1,b,b+1})out.indices.push_back(index);
    }
    if(skirtDepth>0) {
        // Walk the perimeter once, keeping UVs fixed down the skirt. Separate
        // lower vertices preserve the original surface grid and its indexing.
        std::vector<std::uint32_t> edge;
        for(unsigned x=0;x<segments;++x)edge.push_back(x);
        for(unsigned y=0;y<segments;++y)edge.push_back(y*(segments+1)+segments);
        for(unsigned x=segments;x>0;--x)edge.push_back(segments*(segments+1)+x);
        for(unsigned y=segments;y>0;--y)edge.push_back(y*(segments+1));
        const auto lower=static_cast<std::uint32_t>(out.vertices.size());
        for(auto index:edge) {
            auto vertex=out.vertices[index];
            std::array<double,3> p{};
            for(unsigned k=0;k<3;++k)p[k]=out.origin[k]+vertex.position[k];
            const double length=std::hypot(p[0],p[1],p[2]);
            if(!(length>skirtDepth))throw std::invalid_argument("Terrain skirt crosses body centre");
            for(unsigned k=0;k<3;++k)vertex.position[k]=float(p[k]*(1-skirtDepth/length)-out.origin[k]);
            out.vertices.push_back(vertex);
        }
        for(std::uint32_t i=0;i<edge.size();++i) {
            const auto next=(i+1)%static_cast<std::uint32_t>(edge.size());
            for(auto index:{edge[i],edge[next],lower+i,edge[next],lower+next,lower+i})out.indices.push_back(index);
        }
    }
    return out;
}
}
