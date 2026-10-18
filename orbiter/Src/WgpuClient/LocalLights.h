#pragma once
#include "OrbiterAPI.h"
#include "WgpuBridge.h"
#include <cmath>
#include <initializer_list>

// Values are sampled each frame: NASSP can supply live position/intensity refs.
inline bool CockpitLight(const LightEmitter &source, OrbLocalLight &result,
    const VECTOR3 &cameraPosition=_V(0,0,0), const MATRIX3 &cameraRotation=_M(1,0,0,0,1,0,0,0,1),
    DWORD visibility=LightEmitter::VIS_COCKPIT) {
    result={};
    if(!source.IsActive() || !(source.GetVisibility() & visibility))return false;
    const bool spot=source.GetType()==LightEmitter::LT_SPOT;
    if(!spot && source.GetType()!=LightEmitter::LT_POINT)return false;
    const auto &point=static_cast<const PointLight &>(source);
    const double intensity=source.GetIntensity(),range=point.GetRange();
    if(!(intensity>0) || !(range>0))return false;
    auto position=source.GetPosition();
    if(source.IsCameraRelative())position=cameraPosition+mul(cameraRotation,position);
    const auto diffuse=source.GetDiffuseColour(),ambient=source.GetAmbientColour();
    for(int i=0;i<3;++i) {
        result.position[i]=float(position.data[i]);
        result.attenuation[i]=float(point.GetAttenuation()[i]);
        if(result.attenuation[i]<0)return false;
    }
    result.position[3]=float(range);
    result.diffuse[0]=float(diffuse.r*intensity);result.diffuse[1]=float(diffuse.g*intensity);result.diffuse[2]=float(diffuse.b*intensity);
    result.ambient[0]=float(ambient.r*intensity);result.ambient[1]=float(ambient.g*intensity);result.ambient[2]=float(ambient.b*intensity);
    if(spot) {
        const auto &light=static_cast<const SpotLight &>(source);
        if(!(light.GetUmbra()>=0 && light.GetPenumbra()>=light.GetUmbra() && light.GetPenumbra()<=PI))return false;
        auto direction=source.GetDirection();
        if(source.IsCameraRelative())direction=mul(cameraRotation,direction);
        const double norm=std::sqrt(dotp(direction,direction));
        if(!(norm>0) || !std::isfinite(norm))return false;
        for(int i=0;i<3;++i)result.direction[i]=float(direction.data[i]/norm);
        result.direction[3]=float(std::cos(light.GetPenumbra()*0.5));
        result.attenuation[3]=float(std::cos(light.GetUmbra()*0.5));
        result.diffuse[3]=1;
    }
    for(const float *field:{result.position,result.direction,result.attenuation,result.diffuse,result.ambient})
        for(int i=0;i<4;++i)if(!std::isfinite(field[i]))return false;
    return true;
}
