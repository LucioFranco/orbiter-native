#pragma once
#include "GraphicsAPI.h"
#include "ParticleSimulation.h"
namespace orbiter::graphics {
// Graphics client owns this stream through expiry after the vessel detaches it.
class NativeParticleStream final : public oapi::ParticleStream {
    ParticleSimulation simulation_;
    VECTOR3 air_=_V(0,0,0);
    double density_=0;
    bool reverse_=false;
    bool groundValid_=false;
    ParticleGround ground_{};
public:
    NativeParticleStream(oapi::GraphicsClient *client,PARTICLESTREAMSPEC *spec,bool reverse=false)
        :ParticleStream(client,spec),simulation_(specification),reverse_(reverse) {}
    const std::vector<Particle> &Particles() const {return simulation_.Particles();}
    SURFHANDLE Texture() const {return specification.tex;}
    bool Emissive() const {return specification.ltype==PARTICLESTREAMSPEC::EMISSIVE;}
    bool Finished() const {return !hRef && Particles().empty();}
    void Advance(double dt) {
        if(!hRef) {
            simulation_.Advance(dt,false,_V(0,0,0),_V(0,0,0),_V(0,0,0),0,density_,air_);
            if(groundValid_) {
                ground_.point+=ground_.velocity*dt;
                simulation_.ConstrainGround(ground_);
            }
            return;
        }
        auto vessel=oapiGetVesselInterface(hRef);
        if(!vessel || !pos || !dir)return;
        VECTOR3 origin{},direction{},velocity{},omega{},tangent{},airspeed{};
        vessel->Local2Global(*pos,origin);
        vessel->GlobalRot(reverse_?-*dir:*dir,direction);
        vessel->GetGlobalVel(velocity);
        vessel->GetAngularVel(omega);
        vessel->GlobalRot(crossp(omega,*pos),tangent);
        density_=vessel->GetAtmDensity();
        // Air velocity excludes the local emitter's rotational component.
        air_=vessel->GetAirspeedVector(FRAME_GLOBAL,airspeed)?velocity-airspeed:velocity;
        simulation_.Advance(dt,true,origin,velocity+tangent,direction,level?*level:0,density_,air_);
        groundValid_=false;
        if(!Emissive() && vessel->GetSurfaceRef()) {
            VECTOR3 upLocal{},up{},center{},normalLocal{},groundspeed{};
            vessel->HorizonInvRot(_V(0,1,0),upLocal);vessel->GlobalRot(upLocal,up);
            vessel->GetGlobalPos(center);
            vessel->HorizonInvRot(vessel->GetSurfaceNormal(),normalLocal);
            vessel->GlobalRot(normalLocal,ground_.normal);
            const double normalLength=length(ground_.normal),altitude=vessel->GetAltitude(ALTMODE_GROUND);
            if(normalLength>0 && std::isfinite(altitude) && vessel->GetGroundspeedVector(FRAME_GLOBAL,groundspeed)) {
                ground_.normal/=normalLength;ground_.point=center-up*altitude;
                ground_.velocity=velocity-groundspeed;groundValid_=true;
                simulation_.ConstrainGround(ground_);
            }
        }
    }
};
}
