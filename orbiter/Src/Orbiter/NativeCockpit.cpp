#include "NativeCockpit.h"
#include "Orbiter.h"
#include "GraphicsAPI.h"
#include "Vessel.h"
extern Orbiter *g_pOrbiter;
#include <cmath>
#include <algorithm>
#include <limits>
#include <vector>
#include <stdexcept>
namespace orbiter {
NativeCockpit &NativeCockpitState() {static NativeCockpit state;return state;}
void NativeCockpit::Shift(const VECTOR3 &shift) {
    for(auto &[id,area]:areas) {
        const int count=area.shape==1?1:area.shape==2?4:0;
        for(int i=0;i<count;++i)area.points[i]+=shift;
    }
}
void NativeCockpit::Sphere(int id,const VECTOR3 &center,double radius) {
    auto it=areas.find(id);if(it==areas.end())return;
    it->second.shape=std::isfinite(radius)&&radius>0?1:0;it->second.points[0]=center;it->second.radius=radius;
}
void NativeCockpit::Quad(int id,const VECTOR3 &a,const VECTOR3 &b,const VECTOR3 &c,const VECTOR3 &d) {
    auto it=areas.find(id);if(it==areas.end())return;
    it->second.shape=2;it->second.points[0]=a;it->second.points[1]=b;it->second.points[2]=c;it->second.points[3]=d;
}
static bool triangle(const VECTOR3 &o,const VECTOR3 &ray,const VECTOR3 &a,const VECTOR3 &b,const VECTOR3 &c,double &u,double &v,double &t) {
    const auto e1=b-a,e2=c-a,p=crossp(ray,e2);
    const double det=dotp(e1,p);
    if(std::abs(det)<1e-12)return false;
    const auto s=o-a,q=crossp(s,e1);
    u=dotp(s,p)/det;v=dotp(ray,q)/det;t=dotp(e2,q)/det;
    return u>=0 && v>=0 && u+v<=1 && t>=0;
}
std::optional<NativeCockpit::Hit> NativeCockpit::Pick(const VECTOR3 &origin,const VECTOR3 &direction,int event) const {
    if(!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z))return {};
    const double norm=length(direction);
    if(!std::isfinite(norm)||norm<1e-12)return {};
    const auto ray=direction/norm;
    std::optional<Hit> result;
    for(const auto &[id,area]:areas) {
        if(!(area.mouse&event))continue;
        double distance=std::numeric_limits<double>::infinity();VECTOR3 position{};
        if(area.shape==1) {
            const auto to=area.points[0]-origin;const double along=dotp(to,ray);
            const double off2=std::max(0.0,dotp(to,to)-along*along);
            const double disc=area.radius*area.radius-off2;
            if(disc<0 || along+std::sqrt(disc)<0)continue;
            distance=std::max(0.0,along-std::sqrt(disc));position.x=std::sqrt(off2);
        } else if(area.shape==2) {
            double u,v;
            if(triangle(origin,ray,area.points[0],area.points[1],area.points[2],u,v,distance))position=_V(u,v,0);
            else if(triangle(origin,ray,area.points[1],area.points[3],area.points[2],u,v,distance))position=_V(1-v,u+v,0);
            else continue;
        } else continue;
        if(std::isfinite(distance) && (!result || distance<result->distance))result=Hit{id,position,distance};
    }
    return result;
}
}

namespace orbiter {
static void release(NativeCockpit::Area &area) {
    auto gc=g_pOrbiter?g_pOrbiter->GetGraphicsClient():nullptr;
    if(gc) {
        if(area.target && area.background && (area.mapping==PANEL_MAP_BACKGROUND || area.mapping==PANEL_MAP_BGONREQUEST))
            gc->clbkBlt(area.target,area.rect.left,area.rect.top,area.background,0,0,area.rect.right-area.rect.left,area.rect.bottom-area.rect.top);
        if(area.surface && area.surface!=area.target)gc->clbkReleaseSurface(area.surface);
        if(area.background)gc->clbkReleaseSurface(area.background);
        if(area.target)gc->clbkReleaseSurface(area.target);}
}
void NativeCockpit::Reset(OBJHANDLE vessel) {
    for(auto &[id,area]:areas)release(area);
    areas.clear();mfdScreens.clear();std::fill(std::begin(neighbours),std::end(neighbours),-1);owner=vessel;capture[0].reset();capture[1].reset();held[0]=held[1]=false;
}
void NativeCockpit::Register(int id,int mouse) {
    auto it=areas.find(id);if(it!=areas.end())release(it->second);
    areas[id]=Area{};areas[id].mouse=mouse;
}
void NativeCockpit::RegisterDraw(int id,const RECT &rect,int draw,int mouse,int mapping,SURFHANDLE target) {
    Register(id,mouse);auto &area=areas[id];area.draw=draw&0xff;area.rect=rect;area.mapping=mapping;
    auto gc=g_pOrbiter?g_pOrbiter->GetGraphicsClient():nullptr;
    if(!gc || !area.draw)return;
    area.target=target;if(target)gc->clbkIncrSurfaceRef(target);
    const auto w=rect.right-rect.left,h=rect.bottom-rect.top;
    if(w<=0 || h<=0)return;
    if(mapping==PANEL_MAP_DIRECT) {area.surface=target;return;}
    area.surface=gc->clbkCreateSurfaceEx(w,h,OAPISURFACE_RENDERTARGET);
    if(!area.surface)throw std::runtime_error("Cannot create VC redraw surface");
    if(target && (mapping==PANEL_MAP_BACKGROUND || mapping==PANEL_MAP_BGONREQUEST)) {
        area.background=gc->clbkCreateSurfaceEx(w,h,OAPISURFACE_RENDERTARGET);
        if(!area.background || !gc->clbkBlt(area.background,0,0,target,rect.left,rect.top,w,h))throw std::runtime_error("Cannot snapshot VC area background");
    }
}
void NativeCockpit::Trigger(int vcId,int areaId) {
    if(vcId>=0 && vcId!=cockpitId)return;
    auto it=areas.find(areaId);if(it!=areas.end())it->second.userRedraw=true;
}
bool NativeCockpit::BltBackground(int areaId,SURFHANDLE target) {
    auto gc=g_pOrbiter?g_pOrbiter->GetGraphicsClient():nullptr;
    auto it=areas.find(areaId);if(!gc || !target || it==areas.end())return false;
    const auto &area=it->second;
    if(area.mapping!=PANEL_MAP_BGONREQUEST || !area.background)return false;
    return gc->clbkBlt(target,0,0,area.background,0,0,area.rect.right-area.rect.left,area.rect.bottom-area.rect.top);
}
void NativeCockpit::Redraw(Vessel *vessel) {
    auto gc=g_pOrbiter?g_pOrbiter->GetGraphicsClient():nullptr;
    if(!gc || !vessel || owner!=reinterpret_cast<OBJHANDLE>(vessel))return;
    std::vector<int> ids;for(const auto &[id,area]:areas)ids.push_back(id);
    for(int id:ids) {
        auto it=areas.find(id);if(it==areas.end())continue;
        auto &area=it->second;
        const int event=area.initialize?PANEL_REDRAW_INIT:(area.userRedraw?PANEL_REDRAW_USER:(area.mouseRedraw?PANEL_REDRAW_MOUSE:PANEL_REDRAW_ALWAYS));
        area.initialize=false;
        if(event==PANEL_REDRAW_USER)area.userRedraw=false;
        if(event==PANEL_REDRAW_MOUSE)area.mouseRedraw=false;
        if(event!=PANEL_REDRAW_USER && (!area.draw || (!(area.draw&event) && event!=PANEL_REDRAW_INIT)))continue;
        const auto w=area.rect.right-area.rect.left,h=area.rect.bottom-area.rect.top;
        if(area.surface && area.mapping==PANEL_MAP_BACKGROUND && area.background)
            gc->clbkBlt(area.surface,0,0,area.background,0,0,w,h);
        if(area.surface && area.mapping==PANEL_MAP_CURRENT && area.target)
            gc->clbkBlt(area.surface,0,0,area.target,area.rect.left,area.rect.top,w,h);
        const auto target=area.target,surface=area.surface;const auto rect=area.rect;const int mapping=area.mapping;
        ++area.redraws;
        if(vessel->VCRedrawEvent(id,event,surface) && target && surface && mapping!=PANEL_MAP_DIRECT)
            gc->clbkBlt(target,rect.left,rect.top,surface,0,0,w,h);
    }
}
}
