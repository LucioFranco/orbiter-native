#include "AnimationPose.h"
#include <iostream>
using namespace orbiter::graphics;
static void near(const VECTOR3 &a,const VECTOR3 &b) {if(length(a-b)>1e-6)throw std::runtime_error("animation pose mismatch");}
int main() {
    UINT group=0,childGroup=1;
    MGROUP_ROTATE rotation(0,&group,1,_V(1,0,0),_V(0,0,1),float(PI/2));
    ANIMATIONCOMP rotate{0,1,&rotation,nullptr,nullptr,0};ANIMATIONCOMP *list[]={&rotate};
    ANIMATION animation{0,1,1,list};
    auto poses=AnimationPoses(&animation,1,{{0,0},{0,1}});
    near(poses[0].Point(_V(2,0,0)),_V(1,1,0));near(poses[1].Point(_V(2,0,0)),_V(2,0,0));
    animation.defstate=1;animation.state=0;
    near(AnimationPoses(&animation,1,{{0,0}})[0].Point(_V(1,1,0)),_V(2,0,0));
    MGROUP_TRANSLATE translation(0,&childGroup,1,_V(1,0,0));
    ANIMATIONCOMP child{0,1,&translation,&rotate,nullptr,0};ANIMATIONCOMP *children[]={&child};rotate.children=children;rotate.nchildren=1;
    ANIMATIONCOMP *both[]={&rotate,&child};animation={0,1,2,both};
    poses=AnimationPoses(&animation,1,{{0,0},{0,1}});
    near(poses[1].Point(_V(2,0,0)),_V(1,2,0));
    near(translation.shift,_V(1,0,0)); // parent must not alter shared definitions
    near(AnimationPoses(&animation,1,{{0,0},{0,1}})[1].Point(_V(2,0,0)),_V(1,2,0));
    MGROUP_SCALE scale(0,nullptr,0,_V(1,1,1),_V(2,3,4));
    ANIMATIONCOMP scaling{0.2,0.8,&scale,nullptr,nullptr,0};ANIMATIONCOMP *sl[]={&scaling};animation={0,1,1,sl};
    auto pose=AnimationPoses(&animation,1,{{0,0}})[0];
    near(pose.Point(_V(2,2,2)),_V(3,4,5));
    auto expected=_V(0.5,1.0/3,0.25);expected/=length(expected);
    near(pose.Normal(_V(1,1,1)),expected);
    animation.state=0.1;near(AnimationPoses(&animation,1,{{0,0}})[0].Point(_V(2,2,2)),_V(2,2,2));
    std::cout<<"Animation pivots, inverse default state, hierarchy, scale normals and immutability verified\n";
}
