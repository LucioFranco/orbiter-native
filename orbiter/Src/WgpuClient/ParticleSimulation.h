#pragma once
#include "OrbiterAPI.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdint>
namespace orbiter::graphics {
struct Particle {
    VECTOR3 position,velocity;
    double age,lifetime,size,growth,opacity;
    std::uint32_t sprite=0;
};
struct ParticleGround {
    VECTOR3 point,normal,velocity; // world-space point and unit outward normal
};
inline void ContactGround(Particle &particle,const ParticleGround &ground) {
    const double height=dotp(particle.position-ground.point,ground.normal);
    if(height>=0.01)return;
    particle.position+=ground.normal*(0.01-height);
    const double inward=dotp(particle.velocity-ground.velocity,ground.normal);
    if(inward<0)particle.velocity-=ground.normal*inward;
}
class ParticleSimulation {
    PARTICLESTREAMSPEC spec_;
    std::vector<Particle> particles_;
    double phase_=0;
    size_t capacity_;
    std::uint32_t emitted_=0;
    static double Random(std::uint32_t value) {
        value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;
        return double(value)/4294967295.0*2-1;
    }
    static void Move(Particle &p,double dt,double drag,const VECTOR3 &air) {
        const double decay=std::exp(-drag*dt);
        const double integral=drag>1e-12?-std::expm1(-drag*dt)/drag:dt;
        p.position+=air*dt+(p.velocity-air)*integral;
        p.velocity=air+(p.velocity-air)*decay;
        p.age+=dt;p.size=std::max(0.0,p.size+p.growth*dt);
    }
    static double Ramp(double value,double low,double high) {
        if(high<=low)return value>=high?1:0;
        return std::clamp((value-low)/(high-low),0.0,1.0);
    }
public:
    explicit ParticleSimulation(const PARTICLESTREAMSPEC &spec,size_t capacity=16384):spec_(spec),capacity_(capacity) {}
    const std::vector<Particle> &Particles() const {return particles_;}
    void ConstrainGround(const ParticleGround &ground) {
        for(auto &particle:particles_)ContactGround(particle,ground);
    }
    double Opacity(double level,double density) const {
        if(!std::isfinite(level) || !std::isfinite(density))return 0;
        double alpha=std::clamp(level,0.0,1.0);
        switch(spec_.levelmap) {
        case PARTICLESTREAMSPEC::LVL_FLAT:alpha=spec_.lmin;break;
        case PARTICLESTREAMSPEC::LVL_LIN:break;
        case PARTICLESTREAMSPEC::LVL_SQRT:alpha=std::sqrt(alpha);break;
        case PARTICLESTREAMSPEC::LVL_PLIN:alpha=Ramp(level,spec_.lmin,spec_.lmax);break;
        case PARTICLESTREAMSPEC::LVL_PSQRT:alpha=std::sqrt(Ramp(level,spec_.lmin,spec_.lmax));break;
        }
        if(spec_.atmsmap==PARTICLESTREAMSPEC::ATM_FLAT)alpha*=spec_.amin;
        if(spec_.atmsmap==PARTICLESTREAMSPEC::ATM_PLIN)alpha*=Ramp(density,spec_.amin,spec_.amax);
        if(spec_.atmsmap==PARTICLESTREAMSPEC::ATM_PLOG) {
            if(density<=0 || spec_.amin<=0 || spec_.amax<=0)return 0;
            alpha*=Ramp(std::log(density),std::log(spec_.amin),std::log(spec_.amax));
        }
        return std::isfinite(alpha)?alpha:0;
    }
    // Positions and velocities stay double precision in the inertial frame.
    // A detached source stops emission while existing particles finish ageing.
    void Advance(double dt,bool attached,const VECTOR3 &position,const VECTOR3 &velocity,
                 const VECTOR3 &direction,double level,double density,const VECTOR3 &air=_V(0,0,0)) {
        if(!(dt>0) || !std::isfinite(dt))return;
        const double drag=std::isfinite(density)?std::max(0.0,density)*std::max(0.0,spec_.atmslowdown):0;
        for(auto &p:particles_)Move(p,dt,drag,air);
        particles_.erase(std::remove_if(particles_.begin(),particles_.end(),[](const auto &p){return p.age>=p.lifetime;}),particles_.end());
        const double alpha=Opacity(level,density);
        if(!attached || !(level>0) || alpha<=0.01 || !(spec_.srcrate>0) || !std::isfinite(spec_.srcrate) ||
           !(spec_.lifetime>0) || !std::isfinite(spec_.lifetime)){phase_=0;return;}
        const double count=phase_+dt*spec_.srcrate;
        if(!std::isfinite(count)){phase_=0;return;}
        const double total=std::floor(count);phase_=count-total;
        // Retain newest emissions during a long step; never allocate beyond the cap.
        const size_t born=static_cast<size_t>(std::min(total,double(capacity_-particles_.size())));
        const double norm=length(direction);
        const VECTOR3 motion=velocity+(norm>0?direction*(spec_.v0/norm):_V(0,0,0));
        emitted_+=static_cast<std::uint32_t>(std::fmod(total,4294967296.0));
        for(size_t i=0;i<born;++i) {
            const double age=(phase_+i)/spec_.srcrate;
            if(age>=spec_.lifetime)break;
            const auto id=emitted_-1-static_cast<std::uint32_t>(i);
            const double spread=std::max(0.0,spec_.srcspread)*std::abs(spec_.v0);
            const auto noise=_V(Random(id*3+1),Random(id*3+2),Random(id*3+3))*spread;
            Particle p{position-velocity*age,motion+noise,0,spec_.lifetime,
                       std::max(0.0,spec_.srcsize),spec_.growthrate,alpha,1+(id&7)};
            Move(p,age,drag,air);particles_.push_back(p);
        }
    }
};
}
