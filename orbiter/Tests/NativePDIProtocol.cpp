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
#include "Platform/SDLJoystick.h"
#include "NativePDIWindow.h"
#include <memory>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

// Separate bounded observation, yaw diagnostics, and opt-in original pilot
// input procedure. NativeDescent's prior 2000-ft diagnostic remains separate.
// Luminary099 erasables: TIME2/1=0024/25; TIG=3441/42; TTOGO/TFI=3453/54.
// See ERASABLE_ASSIGNMENTS.agc p138-139 and BURN...IGNITION_ROUTINE.agc p744-745.
// CLOKJOB stores TIME2-TIG in TFI (negative before ignition). No memory writes.
namespace {
constexpr double dt = 1.0/60.0;
constexpr const char *scenario = "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m";
void require(bool ok, const std::string &why) { if (!ok) throw std::runtime_error(why); }
std::string quote(const std::string &s) {
    std::ostringstream out; out << '"';
    for (unsigned char c:s) {
        if(c=='"' || c=='\\') out << '\\' << c;
        else if(c<32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    out << '"'; return out.str();
}
std::string section(const std::string &s, const char *begin, const char *end) {
    auto a=s.find(begin), b=s.find(end,a);
    require(a!=std::string::npos && b!=std::string::npos, std::string("missing state section ")+begin);
    return s.substr(a,b-a);
}
std::map<std::string,std::string> fields(const std::string &s) {
    std::map<std::string,std::string> result; std::istringstream in(s); std::string line,key,value;
    while(std::getline(in,line)) { std::istringstream row(line); if(row>>key) {
        std::getline(row,value); auto first=value.find_first_not_of(" \t");
        if(first!=std::string::npos) { value=value.substr(first); if(!value.empty()&&value.back()=='\r')value.pop_back(); result[key]=value; }
    }}
    return result;
}
double signedWord(unsigned word) { require(word<=077777,"invalid AGC word"); return word&040000 ? double(int(word)-077777) : double(word); }
struct State {
    std::map<std::string,std::string> all,display,radar,agc;
    double clock,tig,tfi,countdown;
    unsigned word(const char *name) const {
        auto it=agc.find(name); // NASSP omits zero-valued erasables when saving.
        return it==agc.end()?0:unsigned(std::stoul(it->second,nullptr,8));
    }
    double pair(const char *high,const char *low) const { return (signedWord(word(high))*16384+signedWord(word(low)))/100; }
    explicit State(const std::string &s):all(fields(s)),display(fields(section(s,"DSKY_BEGIN","DSKY_END"))),
        radar(fields(section(s,"LEM_LR_START","LEM_LR_END"))),agc(fields(section(s,"AGC_BEGIN","AGC_END"))) {
        // Original DSKY serialization omits blank registers during display updates.
        // Preserve them as empty observations, rather than inventing digits.
        for(auto key:{"PROG","VERB","NOUN","R1","R2","R3","STATE"}) display.try_emplace(key,"");
        clock=pair("EMEM0024","EMEM0025"); tig=pair("EMEM3441","EMEM3442");
        tfi=pair("EMEM3453","EMEM3454"); countdown=tig-clock;
    }
    bool countdownDisplayComplete() const {
        const auto &r1=display.at("R1"),&r2=display.at("R2"),&r3=display.at("R3");
        auto sign=[](char c){return c=='+' || c=='-';};
        auto digits=[](const std::string &r){return r.size()==6 &&
            (r[0]=='+' || r[0]=='-') && r.find_first_not_of("0123456789",1)==std::string::npos;};
        return digits(r1) && digits(r3) && r2.size()==6 && sign(r2[0]) && r2[3]==' ' &&
            std::isdigit(r2[1]) && std::isdigit(r2[2]) && std::isdigit(r2[4]) && std::isdigit(r2[5]);
    }
    int control(const std::string &name) const { return std::stoi(all.at(name)); }
    bool dsky(const char *verb,const char *noun) const { return display.at("VERB")==verb && display.at("NOUN")==noun; }
};
using PDIProtocolState=State;
// Test-only pointers to existing protected telemetry, without derived-object
// casts, NASSP accessors, or changes to the original controller state.
struct LEMObservation : LEM {
    static bool yawBinding(LEM &lm) {auto member=&LEMObservation::vesim;return (lm.*member).hasActiveBinding(LM_AXIS_INPUT_ACAY);}
    static uint64_t cycles(LEM &lm) {auto member=&LEMObservation::agc;return (lm.*member).vagc.CycleCounter;}
    static int forwardCDU(LEM &lm) {auto member=&LEMObservation::scdu;return (lm.*member).GetAltOutput();}
    static int lateralCDU(LEM &lm) {auto member=&LEMObservation::tcdu;return (lm.*member).GetAltOutput();}
    static VECTOR3 cog(LEM &lm) {auto member=&LEMObservation::currentCoG;return lm.*member;}
    static double lastFuel(LEM &lm) {auto member=&LEMObservation::LastFuelWeight;return lm.*member;}
    static LEM_ACA &aca(LEM &lm) { auto member=&LEMObservation::CDR_ACA; return lm.*member; }
    static GASTA &attitude(LEM &lm) { auto member=&LEMObservation::gasta; return lm.*member; }
    static ATCA &controller(LEM &lm) { auto member=&LEMObservation::atca; return lm.*member; }
    static SCCA2 &relays(LEM &lm) { auto member=&LEMObservation::scca2; return lm.*member; }
};
class Protocol {
    Orbiter &host; VESSEL *sdk; Vessel *vessel; LEM *lm;
    std::ofstream log; unsigned ticks=0,maxTicks,sequence=0; std::optional<double> ignition;
    double previousDps=0; bool sampleEnabled=false,yawInputs=false,yawTargetDriver=false,yawTargetTrace=false;double yawIntegrated=0;
    bool pilotDriver=false,pilotDescent=false,pilotTrace=false,handoffACA=false; int pilotAxis=0,acaTick=0;
    orbiter::platform::SDLJoysticks *joysticks=nullptr; SDL_Joystick *pilotStick=nullptr;
    unsigned neutralTick=0;
    std::function<void(unsigned,bool)> windowPump;
    unsigned yawTraceEnd=0;std::string yawTraceAction;
    double dps() const {
        double result=0; unsigned count=0;
        for(DWORD i=0;i<sdk->GetThrusterCount();++i) {auto th=sdk->GetThrusterHandleByIndex(i);
            if(sdk->GetThrusterMax(th)>40000) {result=sdk->GetThrusterLevel(th);++count;}}
        require(count==1,"expected exactly one original DPS thruster"); return result;
    }
    double elapsed() const {return ticks*dt;}
    void radarGeometry(std::ostream &out,const State &s) const {
        // Read-only reproduction of the original lm_lr.cpp beam transform.
        // This is existing radar lock logic, not a dH acceptance criterion.
        MATRIX3 rotation{};VECTOR3 position{};sdk->GetRotationMatrix(rotation);
        sdk->GetRelativePos(sdk->GetGravityRef(),position);
        const double angle=std::stod(s.radar.at("ANTENNAANGLE"));
        const double alpha=-6*RAD,beta=-angle*RAD;
        const VECTOR3 x=_V(cos(beta),sin(alpha)*sin(beta),-sin(beta)*cos(alpha));
        const VECTOR3 y=_V(0,cos(alpha),sin(alpha));
        const VECTOR3 z=_V(sin(beta),-sin(alpha)*cos(beta),cos(beta)*cos(alpha));
        const VECTOR3 ray=mul(_M(x.x,y.x,z.x,x.y,y.y,z.y,x.z,y.z,z.z),_V(-cos(20.38*RAD),0,-sin(20.38*RAD)));
        const VECTOR3 body=_V(ray.y,ray.x,ray.z);
        const double cosine=dotp(unit(-position),unit(mul(rotation,body)));
        const auto ch12=lm->NativeGuidanceInput(012),ch33=lm->NativeGuidanceInput(033);
        auto flag=[](unsigned channel,int bit){return channel&(1u<<bit)?"true":"false";};
        out<<",\"radar_geometry\":{\"beam_nadir_cosine\":"<<cosine
            <<",\"beam_body\":["<<body.x<<','<<body.y<<','<<body.z<<']'
            <<",\"nadir_global\":["<<unit(-position).x<<','<<unit(-position).y<<','<<unit(-position).z<<']'
            <<",\"altitude_above_radar_origin_m\":"<<sdk->GetAltitude(ALTMODE_GROUND)-3
            <<",\"predicted_slant_range_ft\":";
        if(std::abs(cosine)>1e-12)out<<(sdk->GetAltitude(ALTMODE_GROUND)-3)/cosine/0.3048;else out<<"null";
        out<<",\"position1_discrete\":"<<flag(ch33,LRPos1)
            <<",\"position2_discrete\":"<<flag(ch33,LRPos2)
            <<",\"range_good_discrete\":"<<flag(ch33,LRDataGood)
            <<",\"velocity_good_discrete\":"<<flag(ch33,LRVelocityDataGood)
            <<",\"position2_command\":"<<flag(ch12,LRPositionCommand)
            <<",\"radar_test_switch\":"<<s.control("RadarTestSwitch")<<'}';
    }
    void attitudeTelemetry(std::ostream &out,const State &s) const {
        MATRIX3 r{};sdk->GetRotationMatrix(r);
        auto &aca=LEMObservation::aca(*lm);
        const auto fdai=LEMObservation::attitude(*lm).GetTotalAttitude();
        VECTOR3 position{};sdk->GetRelativePos(sdk->GetGravityRef(),position);
        out<<",\"pgns_fdai_rpy_deg\":["<<fdai.x*DEG<<','<<fdai.y*DEG<<','<<fdai.z*DEG<<']'
            <<",\"gasta_powered\":"<<(LEMObservation::attitude(*lm).IsPowered()?"true":"false")
            <<",\"forward_window_zenith_cosine\":"<<dotp(unit(position),mul(r,_V(0,0,1)));
        const auto &atca=LEMObservation::controller(*lm);auto &relays=LEMObservation::relays(*lm);
        auto output=[&](const char *key){auto it=s.agc.find(key);return it==s.agc.end()?0:std::stoi(it->second);};
        out<<",\"rotation_body_to_global\":[["<<r.m11<<','<<r.m12<<','<<r.m13<<"],["
            <<r.m21<<','<<r.m22<<','<<r.m23<<"],["<<r.m31<<','<<r.m32<<','<<r.m33<<"]]"
            <<",\"aca_proportional_roll_pitch_yaw\":["<<aca.GetACAProp(0)<<','<<aca.GetACAProp(1)<<','<<aca.GetACAProp(2)<<']'
            <<",\"aca_out_of_detent\":"<<(aca.GetOutOfDetent()?"true":"false")
            <<",\"lgc_output_channel_005\":"<<output("OCHAN005")
            <<",\"lgc_output_channel_006\":"<<output("OCHAN006")
            <<",\"atca_primary_voltage_v\":"<<LEMObservation::controller(*lm).GetPrimPowerVoltage()
            <<",\"guidance_relay_k12\":"<<(relays.GetK12()?"true":"false")
            <<",\"guidance_relay_k13\":"<<(relays.GetK13()?"true":"false");
        for(int which=0;which<3;++which) {
            out<<",\""<<(which==0?"pgns_jet_requests":which==1?"jet_driver":"jet_requests")<<"\":[";
            for(int i=0;i<16;++i) {if(i)out<<',';out<<(which==0?int(atca.pgns_jet_request[i]):which==1?int(atca.jet_driver[i]):atca.jet_request[i]);}
            out<<']';
        }
        out<<",\"rcs_jet_levels\":[";
        for(int i=0;i<16;++i) {if(i)out<<',';out<<lm->GetRCSThrusterLevel(i);}out<<']';
    }
    State state() const {std::ostringstream out;vessel->Write(out);return State(out.str());}
    void event(const char *kind,const std::string &action,int row=0,int area=0,int mouse=0,
               const std::string &wanted="",int target=-1) {
        const auto s=state();std::ostringstream record;
        record << std::setprecision(17) << "{\"seq\":" << sequence++ << ",\"event\":" << quote(kind)
            << ",\"action\":" << quote(action)
            << ",\"checklist_yaw_enabled\":" << (yawInputs?"true":"false")
            << ",\"pilot_driver_enabled\":" << (pilotDriver?"true":"false")
            << ",\"pilot_descent_enabled\":" << (pilotDescent?"true":"false")
            << ",\"handoff_aca_enabled\":" << (handoffACA?"true":"false")
            << ",\"aca_phase_tick\":" << acaTick
            << ",\"pilot_aca_axis_s16\":" << pilotAxis
            << ",\"lgc_altitude_ft\":" << (signedWord(s.word("EMEM3534"))*16384+signedWord(s.word("EMEM3535")))/16/.3048
            << ",\"lgc_rhc_yaw_raw_s32\":" << static_cast<int32_t>(s.word("EMEM0043"))
            << ",\"lgc_rhc_yaw_count\":" << signedWord(s.word("EMEM0043") & 077777)
            << ",\"lgc_flagword11_octal\":" << quote(s.agc.count("EMEM0107")?s.agc.at("EMEM0107"):"0")
            << ",\"yaw_target_driver_enabled\":" << (yawTargetDriver?"true":"false")
            << ",\"yaw_integrated_deg\":" << yawIntegrated << ",\"checklist_row\":" << row
            << ",\"area_id\":" << area << ",\"mouse_event\":" << mouse
            << ",\"sim_elapsed_s\":" << elapsed() << ",\"mission_time_s\":" << lm->GetMissionTime()
            << ",\"agc_clock_s\":" << s.clock << ",\"tig_s\":" << s.tig
            << ",\"countdown_s\":" << s.countdown << ",\"lgc_tfi_s\":" << s.tfi
            << ",\"tfi_high_octal\":" << quote(s.agc.count("EMEM3453")?s.agc.at("EMEM3453"):"0")
            << ",\"tfi_low_octal\":" << quote(s.agc.count("EMEM3454")?s.agc.at("EMEM3454"):"0")
            << ",\"actual_ignition_elapsed_s\":" << (ignition?std::to_string(*ignition):"null")
            << ",\"guidance_channel_011\":" << lm->NativeGuidanceInput(011)
            << ",\"guidance_channel_012\":" << lm->NativeGuidanceInput(012)
            << ",\"guidance_channel_032\":" << lm->NativeGuidanceInput(032)
            << ",\"guidance_channel_031\":" << lm->NativeGuidanceInput(031)
            << ",\"keyboard_yaw_right_down\":" << (KEYDOWN(host.KeyState(),OAPI_KEY_NUMPAD3)?"true":"false")
            << ",\"aca_keyboard_yaw_right\":" << lm->aca_keyboard_deflection[THGROUP_ATT_YAWRIGHT-THGROUP_ATT_PITCHUP]
            << ",\"guidance_channel_033\":" << lm->NativeGuidanceInput(033)
            << ",\"dps_level\":" << dps() << ",\"ground_contact\":" << (sdk->GroundContact()?"true":"false")
            << ",\"altitude_m\":" << sdk->GetAltitude(ALTMODE_GROUND)
            << ",\"groundspeed_mps\":" << sdk->GetGroundspeed()
            << ",\"requested_field\":" << quote(wanted) << ",\"requested_state\":" << target
            << ",\"readback\":" << (wanted.empty()?"null":std::to_string(s.control(wanted)))
            << ",\"dsky\":{";
        bool comma=false;
        for(auto key:{"PROG","VERB","NOUN","R1","R2","R3","STATE"}) {
            if(comma)record<<',';comma=true;record<<quote(key)<<':'<<quote(s.display.at(key));}
        record<<"},\"radar\":{";comma=false;
        for(auto key:{"RANGE","ANTENNAANGLE","RANGEGOOD","VELOCITYGOOD","RATE"}) {
            if(comma)record<<',';comma=true;record<<quote(key)<<':'<<quote(s.radar.at(key));}
        record<<"},\"neutral_trace_tick\":"<<neutralTick<<",\"crosspointers\":[";
        for(int i=0;i<2;++i){if(i)record<<',';std::istringstream in(s.all.at(i?"CROSSPOINTER_RIGHT":"CROSSPOINTER_LEFT"));double x,y;require(bool(in>>x>>y),"complete original crosspointer state required");record<<'['<<x<<','<<y<<']';}
        std::ostringstream raw;vessel->Write(raw);
        record<<"],\"dps_gimbals\":[";
        for(int i=0;i<2;++i){if(i)record<<',';auto g=fields(section(raw.str(),i?"DPSGIMBALACTUATOR_ROLL_BEGIN":"DPSGIMBALACTUATOR_PITCH_BEGIN","DPSGIMBALACTUATOR_END"));
            record<<"{\"position\":"<<g.at("POSITION")<<",\"command\":"<<g.at("COMMANDEDPOSITION")<<",\"motor_running\":"<<g.at("MOTORRUNNING")<<'}';}
        record<<"],\"controls\":{";comma=false;
        for(auto key:{"ModeControlPGNSSwitch","ModeControlAGSSwitch","EngineArmSwitch","EngineDescentCommandOverrideSwitch",
            "LandingAntSwitch","PGNS_LDG_RDR_CB","RateScaleSwitch","AltRngMonSwitch","RendezvousRadarRotary",
            "ORDEALFDAI1Switch","ORDEALFDAI2Switch","LeftXPointerSwitch","RightXPointerSwitch",
            "RollSwitch","PitchSwitch","YawSwitch","DeadBandSwitch","THRContSwitch","ATTTranslSwitch","BALCPLSwitch","EngGimbalEnableSwitch"}) {
            if(comma)record<<',';comma=true;record<<quote(key)<<':'<<s.control(key);}
        record<<"},\"active_thrusters\":[";comma=false;
        for(DWORD i=0;i<sdk->GetThrusterCount();++i) {auto th=sdk->GetThrusterHandleByIndex(i);double level=sdk->GetThrusterLevel(th);
            if(level<=0)continue; VECTOR3 dir{};sdk->GetThrusterDir(th,dir);
            if(comma)record<<',';comma=true;record<<"{\"index\":"<<i<<",\"level\":"<<level<<",\"max_thrust_n\":"
                <<sdk->GetThrusterMax(th)<<",\"direction\":["<<dir.x<<','<<dir.y<<','<<dir.z<<"]}";}
        VECTOR3 rates{};sdk->GetAngularVel(rates);
        record << "],\"angular_velocity_body_rad_s\":["<<rates.x<<','<<rates.y<<','<<rates.z<<"]";radarGeometry(record,s);attitudeTelemetry(record,s);
        if(std::string(kind)=="neutral_resume_sample" || std::string(kind)=="handoff_aca_sample") {
            VECTOR3 p{},pmi{};sdk->GetRelativePos(sdk->GetGravityRef(),p);sdk->GetPMI(pmi);
            auto cog=LEMObservation::cog(*lm);auto lastFuel=LEMObservation::lastFuel(*lm);
            record<<",\"lgc_cycle_counter\":"<<LEMObservation::cycles(*lm)
                <<",\"crosspointer_cdu_counts\":["<<LEMObservation::forwardCDU(*lm)<<','<<LEMObservation::lateralCDU(*lm)<<']';
            record<<",\"lm_cog_m\":["<<cog.x<<','<<cog.y<<','<<cog.z<<"],\"pmi_m2\":["<<pmi.x<<','<<pmi.y<<','<<pmi.z<<"]"
                <<",\"relative_position_m\":["<<p.x<<','<<p.y<<','<<p.z<<"],\"altitude_mean_radius_m\":"<<sdk->GetAltitude(ALTMODE_MEANRAD)
                <<",\"last_cog_fuel_weight_kg\":";
            if(std::isfinite(lastFuel))record<<lastFuel;else record<<"null";
        }
        if(handoffACA) {
            VECTOR3 torque=_V(0,0,0);
            for(int i=0;i<16;++i) {
                VECTOR3 pos{},dir{};sdk->GetThrusterRef(lm->th_rcs[i],pos);sdk->GetThrusterDir(lm->th_rcs[i],dir);
                // Match original Vessel.cpp:4035 force x arm convention.
                torque+=crossp(dir,pos)*(sdk->GetThrusterMax(lm->th_rcs[i])*lm->GetRCSThrusterLevel(i));
            }
            record<<",\"rcs_torque_body_nm\":["<<torque.x<<','<<torque.y<<','<<torque.z<<']';
        }
        record << "}\n";log << record.str();log.flush();require(bool(log),"cannot write protocol telemetry");
    }
    void step() {
        require(ticks<maxTicks,"simulated-duration bound reached before checkpoint");
        VECTOR3 before{},after{};if(pilotTrace)sdk->GetAngularVel(before);
        if(windowPump)windowPump(ticks,acaTick>0);
        if(joysticks)joysticks->Poll();
        const double beforeTime=oapiGetSimTime();host.StepSimulation(dt);
        require(host.IsRunning() && oapiGetSimTime()>beforeTime,"counted PDI protocol step must advance simulation");
        ++ticks;
        if(pilotTrace){sdk->GetAngularVel(after);yawIntegrated-=.5*(before.y+after.y)*dt*DEG;}
        const double current=dps();
        if(!ignition && previousDps==0 && current>0) {ignition=elapsed();event("ignition","first_positive_original_dps");}
        previousDps=current;
        if(ticks<=yawTraceEnd)event("yaw_settling_sample",yawTraceAction);
        if(pilotTrace)event("pilot_yaw_sample",yawTraceAction);
        if(yawTargetTrace)event("yaw_target_sample",yawTraceAction);
        if(sampleEnabled && ticks%60==0)event("sample","one_second_observation");
        require(!sdk->GroundContact(),"unexpected ground contact before bounded protocol checkpoint");
    }
    void advance(unsigned count) {for(unsigned i=0;i<count;++i)step();}
    void tap(int area,const std::string &action,int row,unsigned hold=12,double y=.5,int down=PANEL_MOUSE_LBDOWN) {
        event("input_down",action,row,area,down);Vector point(.5,y,0);
        require(vessel->VCMouseEvent(area,down,point),"original control rejected "+action);
        advance(hold);const int up=down==PANEL_MOUSE_RBDOWN?PANEL_MOUSE_RBUP:PANEL_MOUSE_LBUP;
        vessel->VCMouseEvent(area,up,point);event("input_up",action,row,area,up);advance(12);
    }
    void control(const std::string &name,int target,int area,int row,bool rotary=false) {
        auto current=state().control(name);event("state_before",name,row,area,0,name,target);
        for(int attempt=0;current!=target && attempt<3;++attempt) {
            tap(area,name,row,12,target>current?.2:.8,rotary&&target>current?PANEL_MOUSE_RBDOWN:PANEL_MOUSE_LBDOWN);
            current=state().control(name);
        }
        event("state_confirmed",name,row,area,0,name,target);
        require(current==target,"original switch readback mismatch "+name);
    }
    void countdown(double remaining,const char *action) {
        while(state().countdown>remaining)advance(state().countdown-remaining>5?60:6);
        const auto s=state();require(s.countdown>remaining-1,"missed countdown window "+std::string(action));
        event("timing",action);
    }
    void waitDisplay(const char *verb,const char *noun,double seconds,const char *action,bool countdownDigits=false) {
        const double deadline=elapsed()+seconds;
        auto ready=[&](){const auto s=state();return s.dsky(verb,noun) && (!countdownDigits || s.countdownDisplayComplete());};
        while(!ready() && elapsed()<deadline)advance(6);
        require(ready(),"expected DSKY "+std::string(verb)+"/"+noun+" for "+action);
        event("display_confirmed",action);
    }
    void afterIgnition(double seconds,const char *action) {
        require(bool(ignition),"actual DPS ignition not observed");
        while(elapsed()-*ignition<seconds)advance(6);
        event("timing",action);
    }
    void yawRight(double degrees,int row,double deadlineSeconds) {
        // Original NUMPAD3 -> LEM keyboard -> ACA -> LGC/SCS/RCS path.
        // lm_RGA/lmscs maps LM yaw to Orbiter body Y; right yaw is -Y.
        require(sdk->GetAttitudeMode()==RCS_ROT,"checklist yaw requires original rotation input mode");
        const std::string action=degrees<100?"checklist_yaw_right_10":"checklist_yaw_right_174";
        yawIntegrated=0;event("keyboard_down",action,row,OAPI_KEY_NUMPAD3);
        host.NativeKeyEvent(OAPI_KEY_NUMPAD3,true);const double deadline=elapsed()+deadlineSeconds;
        try {
            while(yawIntegrated<degrees && elapsed()<deadline) {
                VECTOR3 before{},after{};sdk->GetAngularVel(before);step();sdk->GetAngularVel(after);
                yawIntegrated-=.5*(before.y+after.y)*dt*DEG;
                require(yawIntegrated> -1,"original right-yaw command produced opposite measured yaw");
            }
        } catch(...) {host.NativeKeyEvent(OAPI_KEY_NUMPAD3,false);event("keyboard_up",action,row,OAPI_KEY_NUMPAD3);throw;}
        host.NativeKeyEvent(OAPI_KEY_NUMPAD3,false);
        yawTraceAction=action;yawTraceEnd=ticks+8*60; // Observe existing steps; add no waits or inputs.
        event("keyboard_up",action,row,OAPI_KEY_NUMPAD3);
        require(yawIntegrated>=degrees,"original yaw did not reach checklist angle within its bounded window");
        advance(12);event("yaw_input_completed",action,row,OAPI_KEY_NUMPAD3);
        require(lm->aca_keyboard_deflection[THGROUP_ATT_YAWRIGHT-THGROUP_ATT_PITCHUP]==0,
                "original keyboard ACA deflection must clear after release");
    }
    void yawRightTarget(double degrees,int row,double deadlineSeconds) {
        // Separate diagnostic driver: same original NUMPAD3 ACA path, early
        // release plus bounded right-only trim pulses and original DAP braking.
        // Relative yaw is the quaternion twist about input-start body Y.
        // PGNS pitch/roll swing remains observable; this is not an FDAI target.
        require(sdk->GetAttitudeMode()==RCS_ROT,"target yaw requires original rotation input mode");
        const std::string action=degrees<100?"target_yaw_right_10":"target_yaw_right_174";
        MATRIX3 start{};sdk->GetRotationMatrix(start);
        double previous=0,angle=0;const double deadline=elapsed()+deadlineSeconds;
        auto observe=[&]() {
            MATRIX3 now{};sdk->GetRotationMatrix(now);
            const auto relative=mul(transp(start),now);
            require(std::hypot(relative.m13-relative.m31,relative.m11+relative.m33)>1e-9,
                    "relative yaw twist is singular");
            const double measured=std::atan2(relative.m13-relative.m31,relative.m11+relative.m33)*DEG;
            angle+=std::remainder(measured-previous,360.0);previous=measured;
        };
        auto rate=[&]() {VECTOR3 value{};sdk->GetAngularVel(value);return -value.y*DEG;};
        auto tick=[&]() {require(elapsed()<deadline,"bounded target yaw did not acquire angle and rate dwell");step();observe();};
        constexpr double tolerance=1.0,rateThreshold=.1;
        // Conservative diagnostic release predictor, fixed before radar runs.
        // 4 deg/s^2 is below the ~5 deg/s^2 braking observed in the exact-input
        // regression. It is not a modified plant parameter or flight value.
        auto stopDistance=[&]() {const double v=std::max(0.0,rate());return v*v/(2*4.0)+.25*v;};
        yawIntegrated=0;yawTraceAction=action;
        event("yaw_target_start",action,row);yawTargetTrace=true;
        for(unsigned pulse=0;pulse<12;++pulse) {
            require(angle<=degrees+tolerance,"target yaw overshoot exceeds diagnostic tolerance");
            const double remaining=degrees-angle;
            event("keyboard_down",action,row,OAPI_KEY_NUMPAD3);host.NativeKeyEvent(OAPI_KEY_NUMPAD3,true);
            const double pulseStart=elapsed();
            const double trimDuration=std::min(.6,std::sqrt(std::max(0.0,remaining)/10.0));
            try {
                do {tick();} while(angle+stopDistance()<degrees-.35 &&
                    (pulse==0 || elapsed()-pulseStart<trimDuration));
            } catch(...) {host.NativeKeyEvent(OAPI_KEY_NUMPAD3,false);event("keyboard_up",action,row,OAPI_KEY_NUMPAD3);throw;}
            host.NativeKeyEvent(OAPI_KEY_NUMPAD3,false);event("keyboard_up",action,row,OAPI_KEY_NUMPAD3);
            // Let the original automatic controller brake. Never send left yaw.
            unsigned quiet=0;
            do {tick();quiet=std::abs(rate())<=rateThreshold?quiet+1:0;} while(quiet<61);
            event("yaw_target_braked",action,row);
            if(std::abs(degrees-angle)<=.75) {
                // Verify terminal angle and rate throughout a full neutral dwell.
                unsigned dwell=0;
                while(dwell<61) {
                    tick();
                    require(std::abs(degrees-angle)<=tolerance && std::abs(rate())<=rateThreshold,
                            "target yaw left diagnostic angle/rate tolerance during dwell");
                    ++dwell;
                }
                event("yaw_target_acquired",action,row);
                for(unsigned i=0;i<8*60;++i)tick();
                event("yaw_target_observation_end",action,row);yawTargetTrace=false;
                return;
            }
        }
        throw std::runtime_error("bounded right-only trim pulse budget exhausted");
    }
    void pilotYaw(double degrees,int row,double deadlineSeconds) {
        // Original SDL -> native VESIM -> original ACA -> Luminary099 RHC path.
        // Pilot yaw is around LM +X/thrust (Orbiter body -Y for right yaw).
        // Body-yaw integration includes braking, unlike the preserved key hold.
        // 19 decoded RHC counts commands ~5.16 deg/s under the original normal scaling:
        // WC=A*(46+4*abs(D))*D, max normal rate 20 at D=42 (AGC p1432/1446).
        require(pilotStick && sdk->GetAttitudeMode()==RCS_ROT,"original pilot rotation input required");
        const std::string action=degrees<100?"pilot_yaw_right_10":"pilot_yaw_right_174";
        const int axis=std::lround((1+10*20.5/42)/13*32767);
        // NASSP writes a signed C counter directly; original AGC SignExtend
        // masks it as a one's-complement word. Raw -20 decodes to -19.
        const double deadline=elapsed()+deadlineSeconds;
        auto rate=[&](){VECTOR3 r{};sdk->GetAngularVel(r);return -r.y*DEG;};
        auto tick=[&](){require(elapsed()<deadline,"pilot yaw did not acquire bounded neutral dwell");step();};
        auto command=[&](int value){pilotAxis=value;require(SDL_SetJoystickVirtualAxis(pilotStick,0,value),"pilot axis rejected");event("pilot_axis_input",action,row);};
        yawIntegrated=0;yawTraceAction=action;event("pilot_yaw_start",action,row);pilotTrace=true;
        try {
            for(unsigned pulse=0;pulse<16;++pulse) {
                require(yawIntegrated<=degrees+1,"pilot yaw overshoot exceeds observer tolerance");
                const double remaining=degrees-yawIntegrated;
                command(axis);const double start=elapsed();
                const double trim=std::min(.4,std::sqrt(std::max(0.0,remaining)/10.0));
                do {tick();} while(yawIntegrated+std::pow(std::max(0.0,rate()),2)/8+.25*std::max(0.0,rate())<degrees-.35 &&
                    (pulse==0 || elapsed()-start<trim));
                command(0);unsigned quiet=0;
                do {tick();quiet=std::abs(rate())<=.1?quiet+1:0;} while(quiet<61);
                event("pilot_yaw_braked",action,row);
                if(std::abs(degrees-yawIntegrated)<=.75) {
                    for(unsigned i=0;i<61+8*60;++i) {
                        tick();require(std::abs(degrees-yawIntegrated)<=1 && std::abs(rate())<=.1,
                            "pilot yaw left angle/rate observer tolerance during neutral dwell");
                        if(i==60)event("pilot_yaw_acquired",action,row);
                    }
                    event("pilot_yaw_observation_end",action,row);pilotTrace=false;return;
                }
            }
            throw std::runtime_error("pilot yaw bounded trim budget exhausted");
        }catch(...){command(0);pilotTrace=false;throw;}
    }
    void handoffControlPulse() {
        // PGNS-45 permits manual ATT HOLD during P64. This early, bounded
        // diagnostic is not the flown TIG+10m landing/control decision.
        require(pilotStick && sdk->GetAttitudeMode()==RCS_ROT && pilotAxis==0,
            "uninterrupted original neutral ACA rotation path required");
        control("ModeControlPGNSSwitch",1,3312,210);
        const auto ready=state();
        for(const auto &[key,value]:std::map<std::string,int>{{"ModeControlPGNSSwitch",1},
                {"RollSwitch",2},{"PitchSwitch",2},{"YawSwitch",2},{"RateScaleSwitch",1},
                {"DeadBandSwitch",0},{"THRContSwitch",1},{"ATTTranslSwitch",1},{"BALCPLSwitch",1}})
            require(ready.control(key)==value,"handoff original switch readiness mismatch: "+key);
        require(ready.display.at("PROG")=="64","original P64 required before diagnostic pulse");
        event("handoff_aca_begin","uninterrupted_att_hold",210);
        auto observe=[&](const char *phase,unsigned frames) {
            for(acaTick=0;acaTick<=int(frames);++acaTick) {
                if(acaTick)step();
                require(height(state())>1200 && !sdk->GroundContact(),"bounded ACA altitude/contact guard");
                event("handoff_aca_sample",phase,210);
            }
        };
        auto command=[&](int axis,const char *action) {
            pilotAxis=axis;require(SDL_SetJoystickVirtualAxis(pilotStick,0,axis),"handoff ACA input rejected");
            event("handoff_aca_input",action,210);
        };
        try {
            observe("neutral_baseline",60);
            command(14823,"yaw_right");observe("yaw_pulse",18);
            command(0,"neutral_release");observe("neutral_release",240);
            event("checkpoint","uninterrupted_aca_pulse_release_complete",210);
        } catch(...) {
            pilotAxis=0;SDL_SetJoystickVirtualAxis(pilotStick,0,0);joysticks->Poll();throw;
        }
    }
    static double height(const State &s) {
        // HCALC=3534/3535, original Luminary099 DP at 2^24 m (AGC p147/876).
        return (signedWord(s.word("EMEM3534"))*16384+signedWord(s.word("EMEM3535")))/16/.3048;
    }
    static double difference(const State &s) {
        const auto &r=s.display.at("R3");
        require(r.size()==6 && (r[0]=='+' || r[0]=='-') && r.find_first_not_of("0123456789",1)==std::string::npos,
                "complete signed N68 dH required before radar decision");return std::stod(r);
    }
    void continuePilotDescent() {
        const auto s=state();const double h=height(s),dh=difference(s);
        require(s.display.at("PROG")=="63" && s.dsky("16","68"),"original P63 V16N68 required for radar decision");
        // Published Apollo11 crew-chart plateau only; no extrapolation or
        // fitted threshold. Rule 5-89 ties the limit to altitude (4/16/1969).
        // Crew chart 9 (6/27), final flown timeline p9 (7/1): 17,500 ft from
        // the 40,700-ft through 27,500-ft tabulated rows. MTM28D item2 notes
        // MIT had not independently verified the published envelope.
        require(h>=27500 && h<=40700,"N68 altitude outside the supported source-chart interval");
        require(s.radar.at("RANGEGOOD")=="1" && s.radar.at("VELOCITYGOOD")=="1" &&
                (lm->NativeGuidanceInput(033)&(1u<<LRDataGood)) &&
                (lm->NativeGuidanceInput(033)&(1u<<LRVelocityDataGood)),"original valid radar required before V57");
        require(std::isfinite(std::stod(s.radar.at("RANGE"))) && std::stod(s.radar.at("RANGE"))>0,"positive finite original radar range required");
        require(std::abs(dh)<=17500,"N68 dH exceeds published crew-chart limit");
        require(!(s.word("EMEM0107")&0200),"LR incorporation must still be inhibited before V57");
        event("radar_decision","source_chart_17500ft_27500_to40700ft",179);
        for(int key:{3401,3410,3412,3418})tap(key,"enable_lr_v57",180);
        require(state().word("EMEM0107")&0200,"original V57 did not permit LR updates");
        event("radar_update_enabled","original_luminary_lrinh_flag_set",180);
        // V57 releases the display back to the original P63 N63. The crew
        // dictionary PGNS-44 explicitly reselects V16N68 after V57.
        for(int key:{3401,3406,3411,3402,3411,3413,3418})tap(key,"post_v57_n68",181);
        waitDisplay("16","68",10,"post_v57_n68");
        advance(30*60);
        require(state().dsky("16","68"),"N68 display lost during post-V57 observation");
        require(std::abs(difference(state()))<std::abs(dh),"N68 dH did not decrease after original V57");
        event("radar_convergence","n68_dh_decreasing_after_v57",181);
        require(host.SaveScenario("NativePDIPilotRadarIncorporated","Original pilot yaw and source-chart V57; N68 convergence",0),"cannot save incorporation checkpoint");
        for(int key:{3401,3406,3411,3402,3411,3408,3418})tap(key,"return_n63",182);
        waitDisplay("16","63",10,"post_update_n63");
        afterIgnition(400,"tig_plus_6m40s");event("observation","planned_throttle_down_readback",185);
        control("ModeControlPGNSSwitch",1,3312,186);
        event("deferred","manual_control_evaluation_requires_pilot_judgment",187);advance(60);
        control("ModeControlPGNSSwitch",2,3312,188);
        afterIgnition(480,"tig_plus_8_minutes");
        for(int key:{3401,3406,3411,3402,3411,3413,3418})tap(key,"monitor_n68",191);
        waitDisplay("16","68",10,"approach_n68");advance(300);
        const double deadline=elapsed()+120;
        while(state().display.at("PROG")!="64" && elapsed()<deadline)advance(6);
        require(state().display.at("PROG")=="64","original P64 transition not observed");
        event("program_transition","original_p64",195);
        control("LandingAntSwitch",0,3303,201);
        tap(3417,"approach_key_rel",202);waitDisplay("06","64",10,"approach_n64");
        afterIgnition(540,"tig_plus_9_minutes");
        require(state().dsky("06","64") && state().display.at("PROG")=="64","original P64 N64 required for planned PRO");
        tap(3416,"approach_pro",205,120);
        while(height(state())>2000) {
            require(state().display.at("PROG")=="64" || state().display.at("PROG")=="65","unexpected guidance program before pilot handoff");
            advance(6);
        }
        event("deferred","ags_deda_223_enter_and_manual_landing_not_validated",207);
        event("checkpoint","pilot_guided_descent_2000ft_manual_handoff_pending",207);
        require(host.SaveScenario("NativePDIPilotHandoff","Source-chart radar updates; original guided descent to manual pilot handoff",0),"cannot save pilot handoff");
        neutralContinuation("uninterrupted");
        event("checkpoint","pilot_neutral_reference_complete",207);
        if(handoffACA)handoffControlPulse();
    }
    void confirmReadiness() {
        const auto s=state();
        const std::map<std::string,int> expected={
            {"DECA_GMBL_AC_CB",1},{"PROP_DISP_ENG_OVRD_LOGIC_CB",1},{"CDR_SCS_AEA_CB",0},
            {"CDR_SCS_ABORT_STAGE_CB",1},{"CDR_SCS_ATCA_CB",1},{"CDR_SCS_AELD_CB",1},{"SCS_ENG_CONT_CB",1},
            {"SCS_ATT_DIR_CONT_CB",1},{"SCS_ENG_START_OVRD_CB",1},{"SCS_DECA_PWR_CB",1},{"SCS_AEA_CB",1},
            {"SCS_ENG_ARM_CB",1},{"SCS_ASA_CB",1},{"SCS_AELD_CB",1},{"SCS_ATCA_CB",1},{"SCS_ABORT_STAGE_CB",1},
            {"SCS_ATCA_AGS_CB",1},{"SCS_DES_ENG_OVRD_CB",1},{"THRContSwitch",1},{"ATTTranslSwitch",1},
            {"BALCPLSwitch",1},{"EngGimbalEnableSwitch",1},{"EngineDescentCommandOverrideSwitch",0},
            {"AbortSwitch",1},{"AbortStageSwitch",1},{"DeadBandSwitch",0},{"RollSwitch",2},{"PitchSwitch",2},
            {"YawSwitch",2},{"CDRManualEngineStop",0},{"LMPManualEngineStop",0}};
        for(const auto &[name,target]:expected) {
            event("state_confirmed",name,0,0,0,name,target);
            require(s.control(name)==target,"scenario readiness mismatch: "+name+" (no automatic repair)");
        }
    }
public:
    Protocol(Orbiter &h,VESSEL *s,Vessel *v,LEM *l,unsigned duration,bool yaw,bool target,bool pilot=false,orbiter::platform::SDLJoysticks *backend=nullptr,SDL_Joystick *stick=nullptr,bool descent=false,const char *logName="PDIProtocol.jsonl",bool aca=false,std::function<void(unsigned,bool)> pump={}):host(h),sdk(s),vessel(v),lm(l),
        log(logName,std::ios::trunc),maxTicks(duration*60),yawInputs(yaw),yawTargetDriver(target),pilotDriver(pilot),pilotDescent(descent),handoffACA(aca),joysticks(backend),pilotStick(stick),windowPump(std::move(pump)) {require(bool(log),"cannot open protocol log");}
    void neutralContinuation(const char *action) {
        require(pilotAxis==0 && height(state())>1500 && !sdk->GroundContact(),"safe neutral handoff observation required");
        previousDps=dps();neutralTick=0;event("neutral_resume_sample",action);
        for(neutralTick=1;neutralTick<=30;++neutralTick){step();event("neutral_resume_sample",action);}
        neutralTick=30;
    }
    bool run() {
        try {
            require(lm->GetApolloNo()==11,"only the supplied Apollo 11 LM protocol is supported");
            std::ifstream config("Missions/ProjectApollo/Apollo 11.cfg");std::ostringstream content;content<<config.rdbuf();
            require(content.str().find("LGCVersion=Luminary099")!=std::string::npos,"Luminary099 config required for observational addresses");
            require(state().display.at("PROG")=="63" && state().dsky("50","18"),"original P63 V50 N18 checkpoint required");
            require(state().countdown>850 && state().countdown<950 && dps()==0,"PDI-minus-15-minute state required");
            event("initial","original_scenario_loaded");
            event("deferred",pilotDriver?"prior_power_setup_visual_checks_AGS_updates_communications_and_full_pilot_fidelity_are_not_validated":"prior_power_setup_visual_checks_AGS_updates_communications_and_manual_yaws_are_not_validated");
            advance(600); // Explicit settling interval; not labelled exact PDI-minus-15 timing.
            sampleEnabled=true;
            control("ModeControlPGNSSwitch",2,3312,50);
            tap(3416,"initial_auto_maneuver_pro",51,120);
            waitDisplay("50","18",60,"initial_auto_maneuver_return");
            if(pilotDriver)pilotYaw(10,55,90);
            else if(yawTargetDriver)yawRightTarget(10,55,90);
            else if(yawInputs)yawRight(10,55,30);
            countdown(300,"pdi_minus_5_minutes");
            control("PGNS_LDG_RDR_CB",1,4181,86);
            control("ORDEALFDAI1Switch",0,3008,88);control("ORDEALFDAI2Switch",0,3009,89);
            countdown(240,"pdi_minus_4_minutes");
            tap(3416,"final_trim_pro",92,120);
            waitDisplay("50","18",120,"final_trim_return");
            require(state().countdown>35,"final trim returned too late");
            tap(3418,"final_trim_enter",93);
            // MIDTOAV can blank the DSKY for tens of seconds after R60 exits.
            // Observe until before V77/setup are due, without moving those inputs.
            const double n62Window=state().countdown-182;
            require(n62Window>0,"no N62 observation window remains before V77");
            waitDisplay("06","62",n62Window,"n62_countdown",true);
            control("RateScaleSwitch",1,3108,118);control("ModeControlAGSSwitch",2,3313,132);
            confirmReadiness();
            countdown(180,"pdi_minus_3_minutes");
            for(int key:{3401,3412,3412,3418})tap(key,"v77",139);
            countdown(35,"pdi_minus_35_seconds");control("EngineArmSwitch",0,3112,143);
            countdown(7.5,"pdi_minus_7_5_seconds");event("observation","ullage_thrusters_only_not_certified_plus_X",146);
            countdown(5,"pdi_minus_5_seconds");waitDisplay("99","62",2,"ignition_request");
            require(state().countdown>2.2,"ignition request appeared too late for bounded PRO hold");
            tap(3416,"ignition_pro",149,120);
            waitDisplay("06","63",10,"ignition_n63"); // Worksheet row 150 precedes override ON.
            control("EngineDescentCommandOverrideSwitch",1,3302,151);
            const double ignitionDeadline=elapsed()+10;
            while(!ignition && elapsed()<ignitionDeadline)advance(1);
            require(bool(ignition),"original DPS ignition not observed");
            afterIgnition(26,"tig_plus_26_seconds");event("observation","throttle_up_observation",154);
            afterIgnition(30,"tig_plus_30_seconds");require(dps()>.5,"DPS throttle-up not observed by actual ignition plus 30 seconds");
            afterIgnition(150,"tig_plus_2m30s");control("AltRngMonSwitch",0,3106,163);
            control("RendezvousRadarRotary",1,3328,164,true);
            afterIgnition(180,"tig_plus_3_minutes");
            if(pilotDriver)pilotYaw(174,169,110);
            else if(yawTargetDriver)yawRightTarget(174,169,110);
            else if(yawInputs)yawRight(174,169,110);
            else event("deferred","manual_yaw_right_174_deg_requires_recorded_pilot_inputs",169);
            afterIgnition(300,"tig_plus_5_minutes");event("n68_request","request_n68",177);
            for(int key:{3401,3406,3411,3402,3411,3413,3418})tap(key,"request_n68",177);
            waitDisplay("16","68",10,"n68_display");advance(300); // Worksheet row 178's five-second observation delay.
            require(state().display.at("NOUN")=="68","N68 display lost before decision checkpoint");
            event("checkpoint","v57_decision_pending_no_operational_dh_envelope",179);
            require(host.SaveScenario("NativePDIProtocolCheckpoint","Observational first N68 checkpoint; V57 not sent",0),"cannot save checkpoint");
            if(pilotDescent)continuePilotDescent();
            return true;
        } catch(const std::exception &e) {
            if(pilotStick){pilotAxis=0;SDL_SetJoystickVirtualAxis(pilotStick,0,0);joysticks->Poll();}
            host.ReleaseNativeKeys();event("blocked",e.what());host.SaveScenario("NativePDIProtocolBlocked",pilotDescent?"Blocked pilot descent; inspect PDIProtocol.jsonl for V57 state":"Blocked observational PDI protocol; no V57 decision",0);
            std::cerr<<"PDI protocol blocked: "<<e.what()<<'\n';return false;
        }
    }
};
}
int main(int argc,char **argv) {try {
    const bool windowed=argc==4 && (std::string(argv[3])=="--pilot-aca-window" || std::string(argv[3])=="--pilot-aca-window-test");
    const bool windowTest=windowed && std::string(argv[3])=="--pilot-aca-window-test";
    require(argc==3 || (argc==4 && (std::string(argv[3])=="--checklist-yaw" || std::string(argv[3])=="--yaw-target" || std::string(argv[3])=="--pilot-yaw" || std::string(argv[3])=="--pilot-descent" || std::string(argv[3])=="--resume-neutral" || (std::string(argv[3])=="--pilot-aca" || windowed))),"verified isolated runtime, duration and optional --checklist-yaw, --yaw-target, --pilot-yaw, --pilot-descent, --resume-neutral or --pilot-aca[ -window | -window-test ] required");
    size_t used=0;const auto duration=std::stoul(argv[2],&used);
    require(used==std::string(argv[2]).size() && duration>=1200 && duration<=(argc==4 && (std::string(argv[3])=="--pilot-descent" || std::string(argv[3])=="--pilot-aca" || windowed)?1800u:1500u),"invalid bounded protocol duration");
    std::filesystem::current_path(argv[1]);
    const bool resume=argc==4 && std::string(argv[3])=="--resume-neutral";
    const bool aca=windowed || (argc==4 && std::string(argv[3])=="--pilot-aca");
    const bool descent=aca || (argc==4 && std::string(argv[3])=="--pilot-descent");
    const bool pilot=resume || descent || (argc==4 && std::string(argv[3])=="--pilot-yaw");
    const auto config=std::filesystem::path("Config/ProjectApollo/Vesim/LM.native.cfg");
    Orbiter host;
    host.Cfg()->Load("Orbiter.cfg");
    require(!host.Cfg()->CfgLogicPrm.bStartPaused,
        "manual PDI protocol requires StartPaused=FALSE before Launch; no implicit step or resume");
    HINSTANCE graphics=nullptr;
    if(windowed) {
        graphics=host.LoadModule("Modules/Plugin","WgpuClient");
        require(graphics && host.GetGraphicsClient() && host.GetGraphicsClient()->clbkInitialise(),"cannot initialize diagnostic graphics");
        require(host.LoadModule("Modules/Plugin","ProjectApolloMFD"),"cannot load original Apollo MFD");
    }
    std::unique_ptr<orbiter::platform::SDLJoysticks> backend;
    std::unique_ptr<PDIWindow> window;

    struct Fixture {
        std::filesystem::path config;bool created=false;SDL_Joystick *stick=nullptr;SDL_JoystickID id=0;
        void release(){if(stick){SDL_SetJoystickVirtualAxis(stick,0,0);SDL_CloseJoystick(stick);stick=nullptr;require(SDL_DetachVirtualJoystick(id),"cannot detach pilot stick");id=0;}if(created){std::filesystem::remove(config);created=false;}}
        ~Fixture(){try{release();}catch(...){}}

    } fixture{config};
    if(pilot){
        require(!std::filesystem::exists(config),"pilot fixture must not overwrite a controller config");
        fixture.created=true;std::ofstream out(config);out<<"\"ACA Yaw\" \"NASSP PDI pilot\" \"\" Axis 0 0 0\n";
        require(bool(out),"cannot create isolated pilot binding");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        backend=std::make_unique<orbiter::platform::SDLJoysticks>();
        SDL_VirtualJoystickDesc desc{};SDL_INIT_INTERFACE(&desc);desc.name="NASSP PDI pilot";desc.naxes=1;
        fixture.id=SDL_AttachVirtualJoystick(&desc);fixture.stick=SDL_OpenJoystick(fixture.id);
        require(fixture.stick && SDL_SetJoystickVirtualAxis(fixture.stick,0,0),"cannot attach neutral pilot stick");
        backend->Poll();
    }
    host.Launch(resume?"NativePDIPilotHandoff":scenario);
    auto sdk=oapiGetFocusInterface();auto vessel=reinterpret_cast<Vessel *>(oapiGetFocusObject());
    require(sdk && std::string(sdk->GetClassName())=="ProjectApollo/LEM","original focused LM required");
    if(windowed) {
        require(host.LoadNativeVirtualCockpit(),"original LM cockpit unavailable");
        auto observe=[&](){
            auto &lm=*static_cast<LEM *>(sdk);PDIWindowState s;
            s.sim=oapiGetSimTime();s.mission=lm.GetMissionTime();s.cycles=LEMObservation::cycles(lm);
            s.yaw=LEMObservation::aca(lm).GetACAProp(2);s.detent=LEMObservation::aca(lm).GetOutOfDetent();
            s.binding=LEMObservation::yawBinding(lm);s.contact=sdk->GroundContact();
            std::ostringstream raw;vessel->Write(raw);PDIProtocolState saved(raw.str());
            s.height=(signedWord(saved.word("EMEM3534"))*16384+signedWord(saved.word("EMEM3535")))/16/.3048;
            MATRIX3 r{};VECTOR3 omega{},position{},velocity{};sdk->GetRotationMatrix(r);sdk->GetAngularVel(omega);
            sdk->GetGlobalPos(position);sdk->GetGlobalVel(velocity);
            s.motion={r.m11,r.m12,r.m13,r.m21,r.m22,r.m23,r.m31,r.m32,r.m33,omega.x,omega.y,omega.z,position.x,position.y,position.z,velocity.x,velocity.y,velocity.z};return s;
        };
        window=std::make_unique<PDIWindow>(host,graphics,observe,windowTest);
        if(windowTest)window->isolationCheck();
    }
    Protocol protocol(host,sdk,vessel,static_cast<LEM *>(sdk),unsigned(duration),argc==4,argc==4 && std::string(argv[3])=="--yaw-target",pilot,backend.get(),fixture.stick,descent,resume?"PDIPilotResume.jsonl":"PDIProtocol.jsonl",aca,windowed?std::function<void(unsigned,bool)>([&](unsigned tick,bool fine){window->pump(tick,fine);}):std::function<void(unsigned,bool)>{});
    if(resume){protocol.neutralContinuation("resumed");return 0;}
    if(!protocol.run())return 2;
    if(windowed) {
        window->handoff([&](){fixture.release();backend->Poll();});
        window->run(windowTest);
    }
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
