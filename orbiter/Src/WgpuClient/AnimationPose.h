#pragma once
#include "OrbiterAPI.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
namespace orbiter::graphics {
struct Pose {
    MATRIX3 r=_M(1,0,0,0,1,0,0,0,1);
    VECTOR3 t{};
    VECTOR3 Point(const VECTOR3 &p) const {return mul(r,p)+t;}
    VECTOR3 Normal(const VECTOR3 &n) const {
        const VECTOR3 a=_V(r.m11,r.m21,r.m31),b=_V(r.m12,r.m22,r.m32),c=_V(r.m13,r.m23,r.m33);
        const double det=dotp(a,crossp(b,c));
        if(std::abs(det)<1e-15)return n;
        const auto v=(crossp(b,c)*n.x+crossp(c,a)*n.y+crossp(a,b)*n.z)/det;
        const double norm=length(v);return norm>1e-15?v/norm:v;
    }
};
inline Pose Compose(const Pose &a,const Pose &b) {
    Pose out;out.r={};
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)for(int k=0;k<3;++k)out.r.data[r*3+c]+=a.r.data[r*3+k]*b.r.data[k*3+c];
    out.t=a.Point(b.t);return out;
}
struct AnimatedGroup {UINT mesh,group;};
// Rebuild from mesh defaults each frame. Work on local transform copies so one
// vessel visual cannot modify the animation definitions used by another visual.
inline std::vector<Pose> AnimationPoses(const ANIMATION *animations,UINT count,const std::vector<AnimatedGroup> &groups) {
    struct Definition {VECTOR3 ref{},vector{};double angle=0;};
    std::map<const MGROUP_TRANSFORM *,Definition> definitions;
    std::function<Definition &(const MGROUP_TRANSFORM *)> definition=[&](const MGROUP_TRANSFORM *t)->Definition & {
        auto [it,created]=definitions.try_emplace(t);if(!created)return it->second;
        auto &d=it->second;
        if(t->Type()==MGROUP_TRANSFORM::ROTATE) {auto p=static_cast<const MGROUP_ROTATE *>(t);d.ref=p->ref;d.vector=p->axis;d.angle=p->angle;}
        if(t->Type()==MGROUP_TRANSFORM::TRANSLATE)d.vector=static_cast<const MGROUP_TRANSLATE *>(t)->shift;
        if(t->Type()==MGROUP_TRANSFORM::SCALE) {auto p=static_cast<const MGROUP_SCALE *>(t);d.ref=p->ref;d.vector=p->scale;}
        return d;
    };
    std::vector<Pose> result(groups.size());
    std::unordered_map<uint64_t,size_t> groupIndex;
    std::unordered_map<UINT,std::vector<size_t>> meshIndex;
    for(size_t i=0;i<groups.size();++i) {
        groupIndex[(uint64_t(groups[i].mesh)<<32)|groups[i].group]=i;
        meshIndex[groups[i].mesh].push_back(i);
    }
    std::function<void(const ANIMATIONCOMP *,const Pose &,unsigned)> apply=[&](const ANIMATIONCOMP *c,const Pose &pose,unsigned depth) {
        if(!c || !c->trans)return;
        if(depth>256)throw std::runtime_error("Animation hierarchy too deep");
        const auto t=c->trans;
        if(t->grp) {
            // LOCALVERTEXLIST is simulation-owned data, not mesh group indices.
            if(t->mesh!=LOCALVERTEXLIST)for(UINT g=0;g<t->ngrp;++g) {
                auto it=groupIndex.find((uint64_t(t->mesh)<<32)|t->grp[g]);
                if(it!=groupIndex.end())result[it->second]=Compose(pose,result[it->second]);
            }
        } else {
            auto it=meshIndex.find(t->mesh);
            if(it!=meshIndex.end())for(auto i:it->second)result[i]=Compose(pose,result[i]);
        }
        for(UINT i=0;i<c->nchildren;++i) {
            const auto child=c->children[i];apply(child,pose,depth+1);
            if(!child || !child->trans)continue;
            auto &d=definition(child->trans);d.ref=pose.Point(d.ref);
            if(child->trans->Type()==MGROUP_TRANSFORM::ROTATE) {d.vector=mul(pose.r,d.vector);const double n=length(d.vector);if(n>0)d.vector/=n;}
            if(child->trans->Type()==MGROUP_TRANSFORM::TRANSLATE)d.vector=mul(pose.r,d.vector);
        }
    };
    for(UINT a=0;a<count;++a) {
        const auto &animation=animations[a];
        if(!std::isfinite(animation.state)||!std::isfinite(animation.defstate))throw std::runtime_error("Nonfinite animation state: "+std::to_string(a));
        for(UINT j=0;j<animation.ncomp;++j) {
            const auto c=animation.comp[animation.state>animation.defstate?j:animation.ncomp-j-1];
            if(!c || !c->trans || c->state1<=c->state0)continue;
            const double lo=std::clamp(animation.defstate,c->state0,c->state1),hi=std::clamp(animation.state,c->state0,c->state1);
            const double delta=(hi-lo)/(c->state1-c->state0);if(delta==0)continue;
            const auto &d=definition(c->trans);Pose pose;
            switch(c->trans->Type()) {
            case MGROUP_TRANSFORM::ROTATE: {
                const double n=length(d.vector);if(n<1e-15)continue;
                const auto axis=d.vector/n;const double angle=delta*d.angle,cs=std::cos(angle),sn=std::sin(angle),v=1-cs;
                const double x=axis.x,y=axis.y,z=axis.z;
                pose.r=_M(cs+x*x*v,x*y*v-z*sn,x*z*v+y*sn,y*x*v+z*sn,cs+y*y*v,y*z*v-x*sn,z*x*v-y*sn,z*y*v+x*sn,cs+z*z*v);
                pose.t=d.ref-mul(pose.r,d.ref);break;
            }
            case MGROUP_TRANSFORM::TRANSLATE:pose.t=d.vector*delta;break;
            case MGROUP_TRANSFORM::SCALE: {
                const double s0=(lo-c->state0)/(c->state1-c->state0),s1=(hi-c->state0)/(c->state1-c->state0);
                for(int k=0;k<3;++k) {const double denom=1+s0*(d.vector.data[k]-1);if(std::abs(denom)<1e-15)throw std::runtime_error("Singular default animation scale");pose.r.data[k*3+k]=(1+s1*(d.vector.data[k]-1))/denom;}
                pose.t=d.ref-mul(pose.r,d.ref);break;
            }
            default:break;
            }
            for(double v:pose.r.data)if(!std::isfinite(v))throw std::runtime_error("Nonfinite animation matrix: "+std::to_string(a)+" component "+std::to_string(j));
            for(double v:pose.t.data)if(!std::isfinite(v))throw std::runtime_error("Nonfinite animation translation: "+std::to_string(a)+" component "+std::to_string(j));
            apply(c,pose,0);
        }
    }
    return result;
}
}
