// Removable, headless observer/input driver. No NASSP headers or state writes.
#include "Orbiter.h"
#include "CoreState.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
void vector(std::ostream &out, const VECTOR3 &v) { out << '[' << v.x << ',' << v.y << ',' << v.z << ']'; }
void observe(std::ostream &out, Orbiter &host, const char *phase, unsigned step, double advanced, bool key) {
    auto sdk = oapiGetFocusInterface();
    VECTOR3 p{},v{},w{},force{},torque{},weight{}; MATRIX3 r{};
    sdk->GetRelativePos(sdk->GetGravityRef(),p); sdk->GetRelativeVel(sdk->GetGravityRef(),v);
    sdk->GetAngularVel(w); sdk->GetRotationMatrix(r);
    const bool haveForce=sdk->GetForceVector(force),haveTorque=sdk->GetTorqueVector(torque),haveWeight=sdk->GetWeightVector(weight);
    const auto &physics=host.Cfg()->CfgPhysicsPrm;
    out << std::setprecision(17) << std::boolalpha << "{\"phase\":\"" << phase << "\",\"step\":" << step
        << ",\"sim_time_s\":" << td.SimT0 << ",\"mjd\":" << td.MJD0 << ",\"advanced_s\":" << advanced
        << ",\"engine_sim_dt_s\":" << td.SimDT << ",\"running\":" << host.IsRunning()
        << ",\"orbit_stabilised\":" << sdk->OrbitStabilised()
        << ",\"nonspherical_gravity_enabled\":" << sdk->NonsphericalGravityEnabled()
        << ",\"keyboard_numpad3\":" << key << ",\"rcs_mode\":" << sdk->GetAttitudeMode()
        << ",\"mass_kg\":" << sdk->GetMass()
        << ",\"physics\":{\"NonsphericalGravitySources\":" << physics.bNonsphericalGrav
        << ",\"DistributedVesselMass\":" << physics.bDistributedMass
        << ",\"RadiationPressure\":" << physics.bRadiationPressure
        << ",\"StabiliseOrbits\":" << physics.bOrbitStabilise
        << ",\"AtmosphericWind\":" << physics.bAtmWind
        << ",\"RefuelOnPad\":" << host.Cfg()->CfgLogicPrm.bPadRefuel
        << ",\"UnlimitedFuel\":" << !host.Cfg()->CfgLogicPrm.bLimitedFuel
        << ",\"ElevationMode\":" << host.Cfg()->CfgVisualPrm.ElevMode
        << ",\"StartPaused_config\":" << host.Cfg()->CfgLogicPrm.bStartPaused
        << "},\"relative_position_m\":";
    vector(out,p);out << ",\"relative_velocity_m_s\":";vector(out,v);
    out << ",\"angular_velocity_body_rad_s\":";vector(out,w);
    out << ",\"rotation_body_to_global\":[";
    for (unsigned i=0;i<9;++i) { if(i)out << ',';out << r.data[i]; }
    out << "],\"force_available\":" << haveForce << ",\"force_body_n\":";vector(out,force);
    out << ",\"torque_available\":" << haveTorque << ",\"torque_body_nm\":";vector(out,torque);
    out << ",\"weight_available\":" << haveWeight << ",\"weight_body_n\":";vector(out,weight);
    out << ",\"thruster_levels\":[";
    for(DWORD i=0;i<sdk->GetThrusterCount();++i) {if(i)out << ',';out << sdk->GetThrusterLevel(sdk->GetThrusterHandleByIndex(i));}
    out << "],\"propellant_mass_kg\":[";
    for(DWORD i=0;i<sdk->GetPropellantCount();++i) {if(i)out << ',';out << sdk->GetPropellantMass(sdk->GetPropellantHandleByIndex(i));}
    out << "]}\n";out.flush();require(bool(out),"cannot write parity observation");
}
}
int main(int argc,char **argv) { try {
    require(argc>=4,"native_parity_probe <runtime> <scenario> <steps> [--aca] [--pause-after] [--dt seconds]");
    std::size_t used=0;const unsigned steps=std::stoul(argv[3],&used);
    require(used==std::string(argv[3]).size() && steps<=559,"step count must be between 0 and 559");
    bool aca=false,pause=false;double dt=1.0/60.0;
    for(int i=4;i<argc;++i) {
        const std::string option=argv[i];
        if(option=="--aca")aca=true;
        else if(option=="--pause-after")pause=true;
        else if(option=="--dt") {require(++i<argc,"--dt requires seconds");used=0;dt=std::stod(argv[i],&used);
            require(used==std::string(argv[i]).size() && dt>0 && std::isfinite(dt),"invalid timestep");}
        else throw std::runtime_error("unknown parity option: "+option);
    }
    require(!aca || (steps==559 && dt==1.0/60.0),"ACA protocol requires 559 steps at 1/60 s");
    std::filesystem::current_path(argv[1]);InitLog("ParityOrbiter.log",false);
    std::ofstream out("parity-state.jsonl");require(bool(out),"cannot create parity observation log");
    Orbiter host;host.Cfg()->Load("Orbiter.cfg");
    require(!host.Cfg()->CfgLogicPrm.bStartPaused,
        "manual-step parity requires StartPaused=FALSE before Launch; the driver owns every step");
    host.Launch(argv[2]);require(td.SimT0==0,"manual-step parity must start at simulation time zero");
    auto save=[&](const char *name) {require(host.SaveScenario(name,"Removable parity experiment; raw state, no flight certification",0),"cannot save parity state");};
    observe(out,host,"loaded",0,0,false);save("ParityLoaded");
    if(aca)require(oapiGetFocusInterface()->GetAttitudeMode()==RCS_ROT,"ACA protocol requires original rotation input mode");
    bool previousKey=false;
    for(unsigned step=1;step<=steps;++step) {
        const bool key=aca && step>=302 && step<=319;
        if(key!=previousKey)host.NativeKeyEvent(OAPI_KEY_NUMPAD3,key);
        previousKey=key;const double before=td.SimT0;host.StepSimulation(dt);
        require(host.IsRunning() && td.SimT0>before,"counted parity step must actually advance simulation");
        const char *phase=step==1?"startup_baseline":step<=301?"coast":step<=319?"yaw_pulse":"neutral_release";
        observe(out,host,phase,step,td.SimT0-before,key);
        if(step==1)save("ParityBaseline");
        if(step==301)save("ParityCoast");
        if(aca && step==319)save("ParityPulse");
    }
    host.ReleaseNativeKeys();
    if(pause) {host.Pause(true);const double before=td.SimT0;host.StepSimulation(dt);
        observe(out,host,"pause_applied",steps,td.SimT0-before,false);}
    save("ParityFinal");
    std::cout << std::setprecision(17) << "Completed " << steps << " steps; simulation time " << td.SimT0 << " s; MJD " << td.MJD0 << '\n';
    return 0;
} catch(const std::exception &error) {std::cerr << "native parity: " << error.what() << '\n';return 1;} }
