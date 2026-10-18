#include "NativeCockpit.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
static void check(bool value,const char *why) {if(!value)throw std::runtime_error(why);}
int main() {
    orbiter::NativeCockpit state;
    state.Register(1,PANEL_MOUSE_LBDOWN);
    state.Quad(1,_V(-1,1,4),_V(1,1,4),_V(-1,-1,4),_V(1,-1,4));
    for(const auto &p:{_V(-0.5,0.5,4),_V(0.5,-0.5,4)}) {
        auto hit=state.Pick(_V(0,0,0),p,PANEL_MOUSE_LBDOWN);
        check(hit && hit->id==1,"quad hit");
        check(std::abs(hit->position.x-(p.x+1)/2)<1e-9 && std::abs(hit->position.y-(1-p.y)/2)<1e-9,"quad UV coordinates in both triangles");
    }
    check(!state.Pick(_V(0,0,0),_V(0,0,-1),PANEL_MOUSE_LBDOWN),"behind eye rejected");
    check(!state.Pick(_V(0,0,0),_V(1,0,0),PANEL_MOUSE_LBDOWN),"parallel ray rejected");
    check(!state.Pick(_V(0,0,0),_V(0,0,1),PANEL_MOUSE_RBDOWN),"mouse mask honored");
    state.Register(2,PANEL_MOUSE_LBDOWN);state.Sphere(2,_V(0,0,2),0.2);
    auto hit=state.Pick(_V(0,0,0),_V(0,0,1),PANEL_MOUSE_LBDOWN);
    check(hit && hit->id==2 && std::abs(hit->distance-1.8)<1e-9,"nearest area wins");
    const VECTOR3 shift=_V(3,-4,12);
    state.Shift(shift);
    auto moved=state.Pick(shift,_V(0,0,1),PANEL_MOUSE_LBDOWN);
    check(moved && moved->id==hit->id && std::abs(moved->distance-hit->distance)<1e-9,"sphere picking follows shifted reference frame");
    moved=state.Pick(shift,_V(0.5,-0.5,4),PANEL_MOUSE_LBDOWN);
    check(moved && moved->id==1 && std::abs(moved->position.x-0.75)<1e-9 && std::abs(moved->position.y-0.75)<1e-9,"quad coordinates survive center-of-gravity shift");
    state.Quad(1,_V(0,0,1),_V(0,0,1),_V(0,0,1),_V(0,0,1));
    state.Sphere(2,_V(0,0,2),0);
    check(!state.Pick(_V(0,0,0),_V(0,0,1),PANEL_MOUSE_LBDOWN),"degenerate areas rejected");
    state.Reset();check(state.areas.empty(),"cockpit reset");
    std::cout<<"Native VC sphere/quad picking, coordinates, filtering and nearest hit verified\n";
}
