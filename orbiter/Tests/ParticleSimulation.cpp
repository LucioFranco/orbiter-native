#include "ParticleSimulation.h"
#include "ParticleLighting.h"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    PARTICLESTREAMSPEC s{};s.srcrate=10;s.lifetime=2;s.srcsize=1;s.growthrate=2;s.v0=5;
    s.levelmap=PARTICLESTREAMSPEC::LVL_LIN;s.atmsmap=PARTICLESTREAMSPEC::ATM_FLAT;s.amin=1;
    orbiter::graphics::ParticleSimulation sim(s,20);
    sim.Advance(1,true,_V(100,0,0),_V(3,0,0),_V(0,1,0),0.5,0);
    require(sim.Particles().size()==10,"emission rate");
    const auto p=sim.Particles().back();
    require(std::abs(p.position.x-100)<1e-10 && std::abs(p.position.y-4.5)<1e-10,"birth timing and inherited motion");
    require(p.opacity==0.5 && std::abs(p.size-2.8)<1e-10,"level opacity and growth");
    sim.Advance(0,false,_V(0,0,0),_V(0,0,0),_V(0,0,1),1,0);
    require(sim.Particles().size()==10,"pause preserves particles");
    sim.Advance(3,false,_V(0,0,0),_V(0,0,0),_V(0,0,1),1,0);
    require(sim.Particles().empty(),"detached particles expire");
    sim.Advance(100,true,_V(0,0,0),_V(0,0,0),_V(0,0,1),1,0);
    require(sim.Particles().size()<=20,"long-step memory bound");
    s.atmsmap=PARTICLESTREAMSPEC::ATM_PLOG;s.amin=0.01;s.amax=1;
    orbiter::graphics::ParticleSimulation atmospheric(s);
    require(std::abs(atmospheric.Opacity(1,0.1)-0.5)<1e-12 && atmospheric.Opacity(1,0)==0,"log density opacity");
    s.atmsmap=PARTICLESTREAMSPEC::ATM_FLAT;s.amin=1;s.atmslowdown=2;s.srcspread=0.2;
    orbiter::graphics::ParticleSimulation whole(s),split(s);
    whole.Advance(1,true,_V(3,0,0),_V(3,0,0),_V(0,1,0),1,0.5,_V(1,0,0));
    for(int i=1;i<=10;++i)split.Advance(0.1,true,_V(0.3*i,0,0),_V(3,0,0),_V(0,1,0),1,0.5,_V(1,0,0));
    auto a=whole.Particles(),b=split.Particles();
    auto older=[](const auto &x,const auto &y){return x.age<y.age;};
    std::sort(a.begin(),a.end(),older);std::sort(b.begin(),b.end(),older);
    require(a.size()==b.size(),"step partition preserves emission count");
    for(size_t i=0;i<a.size();++i)require(length(a[i].position-b[i].position)<1e-9 && length(a[i].velocity-b[i].velocity)<1e-9,"spread and drag agree across timestep partitions");
    require(std::abs(a.back().velocity.x-3)>0.1,"atmospheric drag changes velocity relative to air");
    s.levelmap=PARTICLESTREAMSPEC::LVL_FLAT;s.lmin=0.4;s.amin=0.5;
    orbiter::graphics::ParticleSimulation flat(s);
    require(std::abs(flat.Opacity(1,0)-0.2)<1e-12,"flat mappings use configured opacity constants");
    flat.Advance(1,true,_V(0,0,0),_V(0,0,0),_V(0,1,0),0,0);
    require(flat.Particles().empty(),"zero level disables flat-opacity emitter");
    flat.Advance(0.2,true,_V(0,0,0),_V(0,0,0),_V(0,1,0),1,0);
    require(flat.Particles().size()==2,"flat-opacity emitter starts without accumulating disabled births");
    flat.Advance(0.1,true,_V(0,0,0),_V(0,0,0),_V(0,1,0),0,0);
    require(flat.Particles().size()==2 && flat.Particles().front().age>=0.1,"shutdown retains and ages existing particles");
    PARTICLESTREAMSPEC dust{0,1,5,20,1,2,10,2,PARTICLESTREAMSPEC::DIFFUSE,
        PARTICLESTREAMSPEC::LVL_LIN,0,1,PARTICLESTREAMSPEC::ATM_FLAT,1,1};
    orbiter::graphics::ParticleSimulation lunarDust(dust);
    lunarDust.Advance(1,true,_V(0,0,0),_V(0,0,0),_V(1,0,1),0.6,0);
    require(lunarDust.Particles().size()==5 && lunarDust.Particles().front().opacity==0.6,
            "LM dust specification emits in vacuum at the thrust/altitude-controlled level");
    lunarDust.Advance(0.2,true,_V(0,0,0),_V(0,0,0),_V(1,0,1),0,0);
    require(lunarDust.Particles().size()==5,"LM dust shutdown stops births in vacuum");
    const VECTOR3 lunarCenter=_V(1.5e11,0,0),sun=lunarCenter+_V(1.5e11,0,0);
    const double radius=1737400;
    const std::vector<orbiter::graphics::ParticleOccluder> moon{{lunarCenter,radius}};
    using orbiter::graphics::ParticleBrightness;
    require(ParticleBrightness(false,lunarCenter+_V(radius+0.1,0,0),sun,moon)==0.6f,
            "sunward lunar dust receives sunlight at solar-system coordinates");
    require(ParticleBrightness(false,lunarCenter-_V(radius+0.1,0,0),sun,moon)==0.03f,
            "Moon blocks sunlight from night-side dust");
    require(ParticleBrightness(true,lunarCenter-_V(radius+0.1,0,0),sun,moon)==1,
            "emissive particles remain bright in planetary shadow");
    require(ParticleBrightness(false,lunarCenter+_V(0,radius+0.1,0),sun,moon)==0.6f,
            "grazing sunlight above lunar limb remains visible");
    require(ParticleBrightness(false,_V(0,0,0),_V(10,0,0),{{_V(20,0,0),1}})==0.6f,
            "body beyond light source does not occlude it");
    orbiter::graphics::ParticleGround ground{_V(1.5e11,20,0),unit(_V(0,1,1)),_V(3,0,0)};
    orbiter::graphics::Particle impact{ground.point-ground.normal*2,_V(8,-4,-4),0,2,1,0,1};
    orbiter::graphics::ContactGround(impact,ground);
    require(std::abs(dotp(impact.position-ground.point,ground.normal)-0.01)<1e-5,
            "particle below sloped ground is moved above the plane");
    require(std::abs(dotp(impact.velocity-ground.velocity,ground.normal))<1e-10 && impact.velocity.x==8,
            "contact removes inward velocity and preserves tangential motion");
    const auto contactPosition=impact.position,contactVelocity=impact.velocity;
    orbiter::graphics::ContactGround(impact,ground);
    require(length(impact.position-contactPosition)<1e-8 && length(impact.velocity-contactVelocity)<1e-8,
            "repeated ground contact is stable");
    impact.position=ground.point-ground.normal;impact.velocity=ground.velocity+ground.normal*4;
    orbiter::graphics::ContactGround(impact,ground);
    require(std::abs(dotp(impact.velocity-ground.velocity,ground.normal)-4)<1e-10,
            "outward particle motion survives contact correction");
    std::cout<<"Particle emission, motion, growth, detach and bounded lifetime verified\n";
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
