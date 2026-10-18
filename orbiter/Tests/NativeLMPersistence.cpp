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
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
struct PointerObservation:CrossPointer {
    static double x(CrossPointer &p){auto member=&PointerObservation::display_vel_x;return p.*member;}
    static double y(CrossPointer &p){auto member=&PointerObservation::display_vel_y;return p.*member;}
};
struct MotorObservation:DPSGimbalActuator {
    static bool running(DPSGimbalActuator &g){auto member=&MotorObservation::motorRunning;return g.*member;}
};
static void radarLoad(LEM_LR &radar,const std::string &text){std::ofstream("lm-persistence-input.scn")<<text;std::ifstream in("lm-persistence-input.scn");radar.LoadState(&in,const_cast<char *>("LEM_LR_END"));}
static void motorLoad(DPSGimbalActuator &motor,const std::string &text){std::ofstream("lm-persistence-input.scn")<<text;std::ifstream in("lm-persistence-input.scn");motor.LoadState(&in);}
int main(int argc,char **argv){try{
    require(argc==2,"isolated scratch directory required");std::filesystem::create_directories(argv[1]);std::filesystem::current_path(argv[1]);
    LEM_LR radar;radarLoad(radar,"RANGE 1920.917900454139\nANTENNAANGLE 0\nRANGEGOOD 1\nVELOCITYGOOD 1\nRATE -137.6 -12.0 134.3\nLEM_LR_END\n");
    require(radar.range==1920.917900454139 && radar.rangeGood==1 && radar.velocityGood==1,"RANGEGOOD must not overwrite serialized range");
    radarLoad(radar,"RANGE_EXTRA 0\nRANGE bad\nANTENNAANGLE_EXTRA 24\nRATE 1 2 bad\nRANGEGOOD_EXTRA 0\nVELOCITYGOOD bad\nLEM_LR_END\n");
    require(radar.range==1920.917900454139 && radar.rangeGood==1 && radar.velocityGood==1 && radar.antennaAngle==0 && radar.rate[0]==-137.6 && radar.rate[2]==134.3,"unknown tokens and failed parses must preserve complete radar fields");
    for(const char *key:{"CROSSPOINTER_LEFT","CROSSPOINTER_RIGHT"}){
        CrossPointer pointer;std::string line=std::string(key)+" 18.395761 0.055710";pointer.LoadState(line.data());
        require(PointerObservation::x(pointer)==18.395761 && PointerObservation::y(pointer)==0.055710,"both crosspointer tokens must load their values");
        for(auto bad:{std::string(key)+"_EXTRA 0 0",std::string(key)+" 4 bad"}){pointer.LoadState(bad.data());require(PointerObservation::x(pointer)==18.395761 && PointerObservation::y(pointer)==0.055710,"invalid pointer tokens/partial values must not overwrite readings");}
    }
    DPSGimbalActuator motor;motorLoad(motor,"POSITION -0.062592126667\nCOMMANDEDPOSITION -1\nMOTORRUNNING 1\nDPSGIMBALACTUATOR_END\n");
    require(MotorObservation::running(motor) && motor.GetCmdPosition()==-1 && motor.GetPosition()==-0.062592126667,"running gimbal state must load");
    motorLoad(motor,"MOTORRUNNING_EXTRA 0\nMOTORRUNNING bad\nDPSGIMBALACTUATOR_END\n");require(MotorObservation::running(motor),"unknown/failed motor parse must preserve state");
    motorLoad(motor,"MOTORRUNNING\t0\nDPSGIMBALACTUATOR_END\n");require(!MotorObservation::running(motor),"tab boundary and stopped motor state must load");
    THRUSTER_HANDLE thruster=nullptr;LEM_DPS engine(&thruster);engine.thrustcommand=.42355;
    {std::ofstream out("lm-persistence-engine.scn");engine.SaveState(&out,const_cast<char *>("DPS_BEGIN"),const_cast<char *>("DPS_END"));}
    LEM_DPS loaded(&thruster);
    {std::ifstream in("lm-persistence-engine.scn");loaded.LoadState(&in,const_cast<char *>("DPS_END"));}
    require(loaded.thrustcommand==engine.thrustcommand,"DPS command must survive a fresh save/load before DECA recalculates it");
    {std::ofstream out("lm-persistence-input.scn");out<<"THRUSTCOMMAND_EXTRA 0\nTHRUSTCOMMAND bad\nDPS_END\n";}
    {std::ifstream in("lm-persistence-input.scn");loaded.LoadState(&in,const_cast<char *>("DPS_END"));}
    require(loaded.thrustcommand==engine.thrustcommand,"invalid command must not overwrite saved throttle");
    std::filesystem::remove("lm-persistence-engine.scn");
    std::filesystem::remove("lm-persistence-input.scn");std::cout<<"Original LM radar, both pointers gimbal and throttle exact-token/complete-parse persistence verified\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
