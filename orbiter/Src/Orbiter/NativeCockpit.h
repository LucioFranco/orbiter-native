#pragma once
#include "OrbiterAPI.h"
#include <map>
#include <optional>

class Vessel;
namespace orbiter {
class NativeCockpit {
public:
    struct Area {
        int mouse=0,draw=0,mapping=0;
        RECT rect{};
        SURFHANDLE target=nullptr,surface=nullptr,background=nullptr;
        bool initialize=true,mouseRedraw=false,userRedraw=false;
        unsigned redraws=0;
        int shape=0;
        VECTOR3 points[4]{};
        double radius=0;
    };
    struct Hit { int id; VECTOR3 position; double distance; };
    std::map<int,Area> areas;
    std::map<int,VCMFDSPEC> mfdScreens;
    OBJHANDLE owner=nullptr;
    int cockpitId=0;
    int neighbours[4]={-1,-1,-1,-1};
    std::optional<Hit> capture[2];
    bool held[2]{};
    double mouseX=0,mouseY=0;
    void Reset(OBJHANDLE vessel=nullptr);
    void Register(int id,int mouse);
    void RegisterDraw(int id,const RECT &rect,int draw,int mouse,int mapping,SURFHANDLE target);
    void Redraw(Vessel *vessel);
    void Trigger(int vcId,int areaId);
    bool BltBackground(int areaId,SURFHANDLE target);
    void Shift(const VECTOR3 &shift);
    void Sphere(int id,const VECTOR3 &center,double radius);
    void Quad(int id,const VECTOR3 &p1,const VECTOR3 &p2,const VECTOR3 &p3,const VECTOR3 &p4);
    std::optional<Hit> Pick(const VECTOR3 &origin,const VECTOR3 &direction,int event) const;
};
NativeCockpit &NativeCockpitState();
}
