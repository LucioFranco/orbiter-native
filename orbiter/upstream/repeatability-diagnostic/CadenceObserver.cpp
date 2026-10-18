// External adapter: no NASSP types or physical-vector observers.
#include "Orbiter.h"
#include "Log.h"
#include "Vessel.h"
#include <algorithm>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
// Read only the integer cadence field; no vector getter, cache update or write.
struct BodyRead : Body {
    static int count(const Body& body) { return body.*(&BodyRead::updcount); }
};
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
using Apply=bool(*)(char[256],bool);
using Status=void(*)(unsigned long long[8]);
}
int main(int argc,char **argv){try{
    require(argc==5,"runtime, scenario, steps, off|neutral|pulse required");
    unsigned steps=std::stoul(argv[3]);require(steps>0 && steps<=3601,"bounded steps 1..3601");
    std::string mode=argv[4];require(mode=="off"||mode=="neutral"||mode=="pulse","unknown mode");
    std::filesystem::current_path(argv[1]);InitLog("StepInputOrbiter.log",false);
    Orbiter host;HINSTANCE handle=nullptr;Apply apply=nullptr;Status status=nullptr;
    if(mode!="off"){
        handle=host.LoadModule("Modules/Plugin","StepInputParity");require(handle,"load external module");
        apply=reinterpret_cast<Apply>(dlsym(handle,"StepInputApply"));status=reinterpret_cast<Status>(dlsym(handle,"StepInputStatus"));require(apply&&status,"diagnostic adapter exports");
    }
    host.Launch(argv[2]);
    char cmName[]="Columbia",lmName[]="Eagle";
    auto cm=static_cast<Vessel*>(oapiGetVesselByName(cmName));
    auto lm=static_cast<Vessel*>(oapiGetVesselByName(lmName));
    require(cm&&lm,"original vessels must exist");
    std::ofstream cadence("cadence.tsv");cadence<<"step\tColumbia\tEagle\n";
    int cmPrevious=BodyRead::count(*cm),lmPrevious=BodyRead::count(*lm);
    cadence<<0<<'\t'<<cmPrevious<<'\t'<<lmPrevious<<'\n';
    bool previous=false;unsigned actualDownSteps=0;
    auto prepare=[&](){
        char keys[256]{};require(!apply(keys,true),"module must not consume input");
        if(mode=="neutral")require(std::all_of(keys,keys+256,[](char c){return c==0;}),"neutral callback must leave entire key buffer untouched");
        for(unsigned k=0;k<256;++k)if(k!=OAPI_KEY_NUMPAD3)require(keys[k]==0,"driver must change only numpad3");
        bool desired=(keys[OAPI_KEY_NUMPAD3]&0x80)!=0;if(desired!=previous)host.NativeKeyEvent(OAPI_KEY_NUMPAD3,desired);previous=desired;
    };
    for(unsigned step=1;step<=steps;++step){
        // Stock's startup step precedes pause; its first running input poll is
        // after resumed step2, preparing step3. Neither early step wants yaw.
        if(apply && step>=3)prepare();
        if(previous)++actualDownSteps;
        host.StepSimulation(1.0/60.0);
        int c=BodyRead::count(*cm),l=BodyRead::count(*lm);
        if(c<cmPrevious||l<lmPrevious||step==steps)
            cadence<<step<<'\t'<<c<<'\t'<<l<<'\n';
        cmPrevious=c;lmPrevious=l;
    }
    cadence.close();
    if(apply)prepare(); // Mirror stock's final after-world input poll before exit.
    require(host.SaveScenario("StepInputFinal","External input diagnostic; original scenario state",0),"save final boundary");
    unsigned long long values[8]{};if(status)status(values);
    std::ofstream out("adapter-result.json");out<<"{\"steps\":"<<steps<<",\"mode\":\""<<mode<<"\",\"actual_down_steps\":"<<actualDownSteps<<",\"module_status\":[";
    for(unsigned i=0;i<8;++i){if(i)out<<',';out<<values[i];}out<<"]}\n";out.close();
    require(!status || values[0]==steps,"completed Module callbacks must equal steps");
    require(!status || values[5]==0,"Module recorded callback/input/logger fault");
    unsigned expected=mode=="pulse" ? (steps>301 ? std::min(steps,319u)-301:0):0;
    require(actualDownSteps==expected,"actual yaw input step count");
    if(status)require(values[2]==expected,"Module pre-step pulse count");
    std::cout<<"Completed "<<steps<<" steps, "<<actualDownSteps<<" actual right-yaw steps, "<<mode<<" mode\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
