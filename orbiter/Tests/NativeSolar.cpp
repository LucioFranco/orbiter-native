#include "Orbiter.h"
#include "CelBodyAPI.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==2,"solar runtime required");std::filesystem::current_path(argv[1]);
        Orbiter host;host.Launch("NasspInsertion");
        OBJHANDLE bodies[3]{};const char *names[]={"Sun","Earth","Moon"};
        for(int i=0;i<3;++i) {
            bodies[i]=oapiGetGbodyByName(const_cast<char *>(names[i]));
            auto model=oapiGetCelbodyInterface(bodies[i]);
            require(model && model->bEphemeris(),"native ephemeris module active");
            double now[12]{},later[12]{};
            const int flags=model->clbkEphemeris(oapiGetSimMJD(),EPHEM_TRUEPOS|EPHEM_TRUEVEL,now);
            require(flags&EPHEM_TRUEPOS,"model supplies position");
            model->clbkEphemeris(oapiGetSimMJD()+1,EPHEM_TRUEPOS|EPHEM_TRUEVEL,later);
            bool changed=false;for(int j=0;j<6;++j) {require(std::isfinite(now[j]) && std::isfinite(later[j]),"finite ephemeris");changed|=now[j]!=later[j];}
            require(changed,"ephemeris evolves with epoch");
        }
        auto earth=dynamic_cast<CELBODY2 *>(oapiGetCelbodyInterface(bodies[1]));
        require(earth && earth->GetAtmosphere(),"native Earth atmosphere active");
        VECTOR3 sun{},earthPos{},moon{};
        oapiGetGlobalPos(bodies[0],&sun);oapiGetGlobalPos(bodies[1],&earthPos);oapiGetGlobalPos(bodies[2],&moon);
        require(length(earthPos-sun)>1.4e11 && length(earthPos-sun)<1.6e11,"Earth at solar orbital distance");
        require(length(moon-earthPos)>3.4e8 && length(moon-earthPos)<4.2e8,"Moon at lunar orbital distance");
        for(int i=0;i<3600;++i)host.StepSimulation(1.0/60.0);
        VECTOR3 updated{};oapiGetGlobalPos(bodies[2],&updated);
        require(length(updated-moon)>1000,"Moon propagates during NASSP simulation");
        auto vessel=oapiGetVesselInterface(oapiGetFocusObject());
        vessel->SetAttitudeMode(RCS_LIN);
        host.NativeKeyEvent(OAPI_KEY_NUMPAD2,true);host.StepSimulation(1.0/60.0);
        require(vessel->GetManualControlLevel(THGROUP_ATT_UP,MANCTRL_LINMODE,MANCTRL_KEYBOARD)==1.0,"keyboard translation request reaches NASSP");
        host.ReleaseNativeKeys();host.StepSimulation(1.0/60.0);
        require(vessel->GetManualControlLevel(THGROUP_ATT_UP,MANCTRL_LINMODE,MANCTRL_KEYBOARD)==0.0,"released translation clears request");
        host.NativeKeyEvent(OAPI_KEY_LCONTROL,true);host.NativeKeyEvent(OAPI_KEY_NUMPAD2,true);host.StepSimulation(1.0/60.0);
        require(std::abs(vessel->GetManualControlLevel(THGROUP_ATT_UP,MANCTRL_LINMODE,MANCTRL_KEYBOARD)-0.1)<1e-12,"Ctrl selects low-power translation");
        host.ReleaseNativeKeys();vessel->SetAttitudeMode(RCS_ROT);
        host.NativeKeyEvent(OAPI_KEY_NUMPAD2,true);host.StepSimulation(1.0/60.0);
        require(vessel->GetManualControlLevel(THGROUP_ATT_PITCHUP,MANCTRL_ROTMODE,MANCTRL_KEYBOARD)==0.0,"NASSP-consumed rotation is not applied twice");
        host.ReleaseNativeKeys();host.StepSimulation(1.0/60.0);
        std::cout<<"Native VSOP87/ELP82, Earth atmosphere and 60 seconds of NASSP verified\n";
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
