#include "ExhaustGeometry.h"
#include <iostream>
#include <stdexcept>
static void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main() {
    try {
        VECTOR3 origin=_V(2,3,4),thrust=_V(0,0,1);
        EXHAUSTSPEC spec{};spec.lpos=&origin;spec.ldir=&thrust;
        spec.lsize=10;spec.wsize=2;spec.lofs=3;
        std::array<OrbMeshVertex,8> vertices;
        for(const auto eye:{_V(10,0,0),_V(2,3,20),_V(2,3,1)}) {
            require(ExhaustGeometry(spec,eye,.4f,vertices),"side-on, end-on and coincident camera supported");
            for(const auto &v:vertices) {
                for(float p:v.position)require(std::isfinite(p),"finite billboard coordinates");
                require(v.color[3]==.4f && v.light[0]==1,"thrust opacity and self illumination");
            }
            for(int axis=0;axis<3;++axis) {
                const double base=(vertices[0].position[axis]+vertices[2].position[axis])*0.5;
                const double tip=(vertices[1].position[axis]+vertices[3].position[axis])*0.5;
                require(std::abs(base-(origin-thrust*3).data[axis])<1e-5,"offset follows negative thrust direction");
                require(std::abs(tip-(origin-thrust*13).data[axis])<1e-5,"length follows negative thrust direction");
            }
        }
        require(!ExhaustGeometry(spec,_V(0,0,0),0,vertices),"engine off emits no geometry");
        thrust=_V(0,0,0);require(!ExhaustGeometry(spec,_V(0,0,0),1,vertices),"reject zero direction");
        std::cout<<"Exhaust direction, offset, opacity and degenerate views passed\n";
    } catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
