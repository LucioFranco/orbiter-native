#include "Orbiter.h"
#include "Vessel.h"
#include "nasspdefs.h"
#include "soundlib.h"
#include "toggleswitch.h"
#include "apolloguidance.h"
#include "lm_channels.h"
#include "mcc.h"
#include "mccvessel.h"
#include "LEM.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cmath>
// Manual mission-development probe. A completed run is telemetry, not a landing assertion.
int main(int argc,char **argv) {try {
    if(argc<3)throw std::runtime_error("runtime, simulated duration and optional probe controls required");
    bool prepare=false,proceed=false,approach=false,stopAtContact=false;double radarLimit=0;
    std::string scenario="NasspLM",saveName="NativeDescentCheckpoint";
    for(int arg=3;arg<argc;++arg) {
        const std::string option=argv[arg];
        if(option=="--prepare-pdi")prepare=proceed=true;
        else if(option=="--proceed-attitude")proceed=true;
        else if(option=="--proceed-approach")approach=true;
        else if(option=="--stop-at-contact")stopAtContact=true;
        else if(option=="--scenario" && arg+1<argc)scenario=argv[++arg];
        else if(option=="--save" && arg+1<argc)saveName=argv[++arg];
        else if(option=="--radar-limit-ft" && arg+1<argc) {
            radarLimit=std::stod(argv[++arg]);
            if(!std::isfinite(radarLimit) || radarLimit<=0 || radarLimit>=99999)throw std::runtime_error("invalid diagnostic radar limit");
        } else throw std::runtime_error("unknown or incomplete probe option");
    }
    if((approach || radarLimit || stopAtContact) && !prepare)throw std::runtime_error("later probe controls require --prepare-pdi");
    const double duration=std::stod(argv[2]);
    if(!std::isfinite(duration) || duration<=0 || duration>7200)throw std::runtime_error("duration must be in (0,7200] seconds");
    std::filesystem::current_path(argv[1]);Orbiter host;host.Cfg()->Load("Orbiter.cfg");
    if(host.Cfg()->CfgLogicPrm.bStartPaused)throw std::runtime_error("manual descent requires StartPaused=FALSE before Launch");
    host.Launch(scenario.c_str());
    auto sdk=oapiGetFocusInterface();auto vessel=reinterpret_cast<Vessel *>(oapiGetFocusObject());
    if(!sdk)throw std::runtime_error("LM not loaded");
    double simulatedElapsed=0;
    bool firstContact=false;
    auto step=[&]() {
        VECTOR3 before{};sdk->GetGroundspeedVector(FRAME_HORIZON,before);
        const double speed=sdk->GetGroundspeed();const bool contact=sdk->GroundContact();
        const double beforeTime=oapiGetSimTime();host.StepSimulation(1.0/60.0);
        if(!host.IsRunning() || oapiGetSimTime()<=beforeTime)throw std::runtime_error("counted descent step must advance simulation");
        simulatedElapsed+=1.0/60.0;
        if(!firstContact && !contact && sdk->GroundContact()) {
            firstContact=true;
            std::cout<<"FIRST_CONTACT sim_elapsed="<<simulatedElapsed<<" precontact_vertical_speed="<<before.y
                     <<" precontact_groundspeed="<<speed<<" soft_contact="<<(std::abs(before.y)<3 && speed<3)<<'\n';
            if(stopAtContact) {
                Vector button(0.5,0.5,0);
                if(!vessel->VCMouseEvent(3513,PANEL_MOUSE_LBDOWN,button))throw std::runtime_error("original engine STOP button rejected");
                vessel->VCMouseEvent(3513,PANEL_MOUSE_LBUP,button);
                std::cout<<"Original engine STOP applied after first contact\n";
            }
        }
    };
    auto snapshot=[&]() {std::ostringstream out;vessel->Write(out);return out.str();};
    auto section=[](const std::string &state,const char *begin,const char *end) {
        const auto a=state.find(begin),b=state.find(end,a);
        return a==std::string::npos || b==std::string::npos?std::string{}:state.substr(a,b-a);
    };
    auto field=[](const std::string &block,const char *wanted) {
        std::istringstream input(block);std::string line,key,value;
        while(std::getline(input,line)){std::istringstream row(line);if(row>>key>>value && key==wanted)return value;}
        return std::string{};
    };
    auto tap=[&](int area) {
        Vector point(0.5,0.5,0);
        if(!vessel->VCMouseEvent(area,PANEL_MOUSE_LBDOWN,point))throw std::runtime_error("probe DSKY key rejected");
        for(int i=0;i<12;++i)step();vessel->VCMouseEvent(area,PANEL_MOUSE_LBUP,point);
        for(int i=0;i<12;++i)step();
    };
    auto report=[&](double elapsed) {
        std::ostringstream out;vessel->Write(out);const auto state=out.str();
        const auto a=state.find("DSKY_BEGIN"),b=state.find("DSKY_END",a);
        double thrust=0;
        for(DWORD i=0;i<sdk->GetThrusterCount();++i) {
            auto handle=sdk->GetThrusterHandleByIndex(i);
            if(sdk->GetThrusterMax(handle)>40000)thrust=sdk->GetThrusterLevel(handle);
        }
        VECTOR3 groundVelocity{};
        sdk->GetGroundspeedVector(FRAME_HORIZON,groundVelocity);
        std::cout<<"elapsed="<<elapsed<<" sim_elapsed="<<simulatedElapsed<<" vertical_speed="<<groundVelocity.y<<" altitude="<<sdk->GetAltitude(ALTMODE_GROUND)
                 <<" groundspeed="<<sdk->GetGroundspeed()<<" dps="<<thrust<<" contact="<<sdk->GroundContact()<<'\n';
        if(a!=std::string::npos && b!=std::string::npos)std::cout<<state.substr(a,b-a);
        const auto lrStart=state.find("LEM_LR_START"),lrEnd=state.find("LEM_LR_END",lrStart);
        if(lrStart!=std::string::npos && lrEnd!=std::string::npos)std::cout<<state.substr(lrStart,lrEnd-lrStart);
        auto lm=static_cast<LEM *>(sdk);
        std::cout<<"channel012="<<std::oct<<lm->NativeGuidanceInput(012)
                 <<" channel033="<<lm->NativeGuidanceInput(033)<<std::dec<<'\n';
        // Evaluate the unchanged NASSP range-beam geometry for both antenna positions.
        // This separates pointing/lock from the LGC's radar-incorporation procedure.
        MATRIX3 rotation{};VECTOR3 position{};sdk->GetRotationMatrix(rotation);
        sdk->GetRelativePos(sdk->GetGravityRef(),position);
        for(double angle:{24.0,0.0}) {
            const double alpha=-6*RAD,beta=-angle*RAD;
            const VECTOR3 x=_V(cos(beta),sin(alpha)*sin(beta),-sin(beta)*cos(alpha));
            const VECTOR3 y=_V(0,cos(alpha),sin(alpha));
            const VECTOR3 z=_V(sin(beta),-sin(alpha)*cos(beta),cos(beta)*cos(alpha));
            const VECTOR3 ray=mul(_M(x.x,y.x,z.x,x.y,y.y,z.y,x.z,y.z,z.z),_V(-cos(20.38*RAD),0,-sin(20.38*RAD)));
            const double cosine=dotp(unit(-position),unit(mul(rotation,_V(ray.y,ray.x,ray.z))));
            std::cout<<"radar_geometry_angle="<<angle<<" cosine="<<cosine
                     <<" predicted_range_ft="<<(sdk->GetAltitude(ALTMODE_GROUND)-3)/cosine/0.3048<<'\n';
        }
        std::cout.flush();
    };
    if(std::string(sdk->GetClassName())!="ProjectApollo/LEM")throw std::runtime_error("unexpected vessel class");
    auto lm=static_cast<LEM *>(sdk);
    if(!lm)throw std::runtime_error("expected real LM module");
    if(proceed) {
        for(int i=0;i<600;++i)step();
        if(prepare) {
            for(const auto &control : {std::pair<int,double>{3312,0.2},{3112,0.8},{4181,0.5},{3303,0.8}}) {
                Vector switchPoint(0.5,control.second,0);
                if(!vessel->VCMouseEvent(control.first,PANEL_MOUSE_LBDOWN,switchPoint))throw std::runtime_error("PDI preparation switch rejected");
                for(int i=0;i<30;++i)step();
                vessel->VCMouseEvent(control.first,PANEL_MOUSE_LBUP,switchPoint);
            }
        }
        Vector point(0.5,0.5,0);
        if(!vessel->VCMouseEvent(3416,PANEL_MOUSE_LBDOWN,point))throw std::runtime_error("DSKY PRO callback rejected");
        std::cout<<"PRO down channel032="<<std::oct<<lm->NativeGuidanceInput(032)<<std::dec<<std::endl;
        // Hold long enough to exclude pulse-duration ambiguity.
        for(int i=0;i<120;++i)step();
        if(!vessel->VCMouseEvent(3416,PANEL_MOUSE_LBUP,point))throw std::runtime_error("DSKY PRO release rejected");
        std::cout<<"PRO up channel032="<<std::oct<<lm->NativeGuidanceInput(032)<<std::dec<<std::endl;
    }
    report(0);
    bool ignitionConfirmed=false,radarRequested=false,radarAccepted=false,approachReleased=false,approachProceeded=false;
    double ignitionTime=0;
    const auto ticks=static_cast<unsigned>(std::ceil(duration*60));
    for(unsigned i=1;i<=ticks;++i) {
        step();
        if(i%1800==0 || i==ticks)report(i/60.0);
        if(prepare && !ignitionConfirmed && i%60==0) {
            std::ostringstream out;vessel->Write(out);const auto state=out.str();
            const auto start=state.find("DSKY_BEGIN"),end=state.find("DSKY_END",start);
            const auto display=state.substr(start,end-start);
            if(display.find("VERB 99")!=std::string::npos) {
                Vector key(0.5,0.5,0);
                if(!vessel->VCMouseEvent(3416,PANEL_MOUSE_LBDOWN,key))throw std::runtime_error("ignition PRO rejected");
                for(int hold=0;hold<120;++hold)step();
                vessel->VCMouseEvent(3416,PANEL_MOUSE_LBUP,key);
                ignitionConfirmed=true;
                ignitionTime=simulatedElapsed;
                std::cout<<"Confirmed ignition request with PRO\n";
            }
        }
        if(prepare && ignitionConfirmed && !radarRequested && simulatedElapsed-ignitionTime>=300) {
            // Checklist PDI: V16 N68 displays radar/guidance altitude disagreement before V57 approval.
            Vector key(0.5,0.5,0);
            for(int area : {3401,3406,3411,3402,3411,3413,3418}) {
                if(!vessel->VCMouseEvent(area,PANEL_MOUSE_LBDOWN,key))throw std::runtime_error("radar display key rejected");
                for(int hold=0;hold<12;++hold)step();
                vessel->VCMouseEvent(area,PANEL_MOUSE_LBUP,key);
                for(int gap=0;gap<12;++gap)step();
            }
            radarRequested=true;
            std::cout<<"Requested N68 radar comparison; radar update not yet accepted\n";
        }
        if(prepare && ignitionConfirmed && i%60==0 && (radarLimit || approach)) {
            const auto state=snapshot();const auto display=section(state,"DSKY_BEGIN","DSKY_END");
            const auto radar=section(state,"LEM_LR_START","LEM_LR_END");
            if(radarLimit && radarRequested && !radarAccepted && field(display,"NOUN")=="68" &&
               field(radar,"RANGEGOOD")=="1" && field(radar,"VELOCITYGOOD")=="1") {
                const auto difference=field(display,"R3");
                if(!difference.empty() && std::abs(std::stod(difference))<=radarLimit) {
                    for(int key:{3401,3410,3412,3418})tap(key); // Original V57 radar-update acceptance.
                    radarAccepted=true;
                    std::cout<<"RADAR_ACCEPTED sim_elapsed="<<simulatedElapsed<<" prior_dh_ft="<<difference
                             <<" diagnostic_limit_ft="<<radarLimit<<'\n';
                }
            }
            if(approach && field(display,"PROG")=="64") {
                if(!approachReleased) {
                    Vector switchPoint(0.5,0.8,0); // Original LDG ANT HOVER control.
                    if(!vessel->VCMouseEvent(3303,PANEL_MOUSE_LBDOWN,switchPoint))throw std::runtime_error("hover antenna control rejected");
                    vessel->VCMouseEvent(3303,PANEL_MOUSE_LBUP,switchPoint);
                    tap(3417);approachReleased=true; // KEY REL restores the guidance display.
                    std::cout<<"Approach antenna and KEY REL applied\n";
                } else if(!approachProceeded && field(display,"NOUN")=="64" && field(display,"VERB")=="06") {
                    tap(3416);approachProceeded=true;std::cout<<"Approach PRO applied\n";
                }
            }
        }
        if(prepare && i==1800) {
            std::ostringstream out;vessel->Write(out);const auto state=out.str();
            const auto start=state.find("DSKY_BEGIN"),end=state.find("DSKY_END",start);
            const auto display=state.substr(start,end-start);
            if(display.find("VERB 50")!=std::string::npos && display.find("NOUN 18")!=std::string::npos) {
                // Apollo 11 LM checklist PDI rows 91-93: ENTER after final trim advances to N62.
                // This probe accepts early to isolate the guidance transition, not to validate checklist timing.
                Vector key(0.5,0.5,0);vessel->VCMouseEvent(3418,PANEL_MOUSE_LBDOWN,key);
                for(int hold=0;hold<120;++hold)step();
                vessel->VCMouseEvent(3418,PANEL_MOUSE_LBUP,key);
                std::cout<<"Accepted attitude with ENTER\n";report(i/60.0);
            }
        }
    }
    if(!host.SaveScenario(saveName.c_str(),"Native descent investigation checkpoint",0))throw std::runtime_error("cannot save checkpoint");
    return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}
