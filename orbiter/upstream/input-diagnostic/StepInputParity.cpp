// External diagnostic: public Module callbacks only; no NASSP headers/getters.
#ifdef _WIN32
#include "WinPrelude.h"
#include "ModuleAPI.h"
extern "C" int _fltused = 0;
void operator delete(void *p) noexcept { if (p) HeapFree(GetProcessHeap(), 0, p); }
void operator delete(void *p, size_t) noexcept { if (p) HeapFree(GetProcessHeap(), 0, p); }
#else
#include "Orbitersdk.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#endif

namespace {
constexpr unsigned YawKey=0x51;
static_assert(sizeof(oapi::ModuleNV)==16 && sizeof(oapi::Module)==24,"public Module layout");
void environment(const char *key,char *out,unsigned cap) {
    out[0]=0;
#ifdef _WIN32
    auto n=GetEnvironmentVariableA(key,out,cap);if(n>=cap)out[0]=0;
#else
    auto value=std::getenv(key);if(!value)return;unsigned i=0;
    for(;value[i] && i+1<cap;++i)out[i]=value[i];out[i]=0;
    if(value[i])out[0]=0;
#endif
}
bool equal(const char *a,const char *b){while(*a && *a==*b){++a;++b;}return *a==*b;}
struct Log {
#ifdef _WIN32
    HANDLE file=nullptr;
#else
    std::FILE *file=nullptr;
#endif
    bool failed=false;
    void open() {
        char path[2048];environment("STEP_INPUT_TRACE",path,sizeof path);
        if(!path[0]){failed=true;return;}
#ifdef _WIN32
        file=CreateFileA(path,0x40000000,1,nullptr,1,0,nullptr);
        if(file==reinterpret_cast<HANDLE>(static_cast<size_t>(-1))){file=nullptr;failed=true;}
#else
        file=std::fopen(path,"wx");if(!file)failed=true;
#endif
    }
    void write(const char *data,unsigned n) {
        if(!file){failed=true;return;}
#ifdef _WIN32
        DWORD written=0;if(!WriteFile(file,data,n,&written,nullptr)||written!=n)failed=true;
#else
        if(std::fwrite(data,1,n,file)!=n || std::fflush(file))failed=true;
#endif
    }
    void close(){if(!file)return;
#ifdef _WIN32
        if(!CloseHandle(file))failed=true;
#else
        if(std::fclose(file))failed=true;
#endif
        file=nullptr;
    }
};
struct Line {
    char buf[256];unsigned used=0;
    void text(const char *s){while(*s && used<sizeof buf)buf[used++]=*s++;}
    void integer(unsigned long long x){char tmp[32];unsigned n=0;do{tmp[n++]=char('0'+x%10);x/=10;}while(x);while(n)buf[used++]=tmp[--n];}
    void bits(double value){const unsigned char *p=reinterpret_cast<const unsigned char *>(&value);text("0x");const char digits[]="0123456789abcdef";for(int i=7;i>=0;--i){buf[used++]=digits[p[i]>>4];buf[used++]=digits[p[i]&15];}}
};
class StepInput:public oapi::Module {
public:
    explicit StepInput(HINSTANCE h):oapi::Module(h){}
    Log log;bool pulse=false;unsigned long long completed=0,prepared=0,activeFrames=0,downCalls=0,inputCalls=0,faults=0;bool desired=false;
    void clbkSimulationStart(RenderMode) override {
        char mode[32];environment("STEP_INPUT_MODE",mode,sizeof mode);pulse=equal(mode,"pulse");
        completed=prepared=activeFrames=downCalls=inputCalls=faults=0;desired=false;log.open();
        Line line;line.text(pulse?"start\tpulse\n":"start\tneutral\n");log.write(line.buf,line.used);
    }
    void clbkSimulationEnd() override {
        Line line;line.text("end\t");line.integer(completed);line.text("\t");line.integer(prepared);line.text("\t");line.integer(activeFrames);line.text("\t");line.integer(downCalls);line.text("\t");line.integer(faults|(log.failed?16:0));line.text("\n");log.write(line.buf,line.used);log.close();
    }
    void clbkPreStep(double,double dt,double) override {
        if(dt!=1.0/60.0)faults|=1;
        if(completed>=3601)faults|=2;
        bool want=pulse && completed>=301 && completed<319;
        if(want){++activeFrames;if(prepared!=completed+1 || !desired)faults|=4;}
        if(pulse && completed>=319 && desired)faults|=8;
    }
    void clbkPostStep(double simt,double dt,double mjd) override {
        ++completed;
        if(completed<=3 || completed==301 || completed==319 || completed==320 || completed==559 || completed==3601) {
            Line line;line.text("post\t");line.integer(completed);line.text("\t");line.bits(simt);line.text("\t");line.bits(dt);line.text("\t");line.bits(mjd);line.text("\n");log.write(line.buf,line.used);
        }
    }
    bool clbkProcessKeyboardImmediate(char keys[256],bool running) override {
        if(!running)return false;
        ++inputCalls;prepared=completed+1;
        if(!pulse)return false; // byte-for-byte untouched neutral buffer
        desired=completed>=301 && completed<319;
        if(desired) {
            const unsigned otherKeys[]={0x1du,0x9du,0x38u,0xb8u,0x2au,0x36u,0x4fu,0x50u,0x4bu,0x4du,0x47u,0x48u,0x49u};for(unsigned k:otherKeys)if(keys[k]&0x80)faults|=32;
            char name[]="Eagle";if(oapiGetFocusObject()!=oapiGetVesselByName(name))faults|=64;
        }
        if(faults || log.failed)desired=false;
        keys[YawKey]=desired?static_cast<char>(0x80):0;
        if(desired)++downCalls;
        if((prepared>=301 && prepared<=321) || prepared==560) {
            Line line;line.text("input\t");line.integer(prepared);line.text("\t");line.integer(desired?1:0);line.text("\t");line.integer(faults|(log.failed?16:0));line.text("\n");log.write(line.buf,line.used);
        }
        return false; // normal Orbiter and NASSP immediate processing continues
    }
};
StepInput *module=nullptr;
}
DLLCLBK void InitModule(HINSTANCE h){
#ifdef _WIN32
    void *memory=HeapAlloc(GetProcessHeap(),0,sizeof(StepInput));if(!memory)return;module=new(memory)StepInput(h);
#else
    module=new StepInput(h);
#endif
    oapiRegisterModule(module);
}
DLLCLBK void ExitModule(HINSTANCE){delete module;module=nullptr;}
DLLCLBK bool StepInputApply(char keys[256],bool running){return module && module->clbkProcessKeyboardImmediate(keys,running);}
DLLCLBK void StepInputStatus(unsigned long long out[8]){if(!module)return;out[0]=module->completed;out[1]=module->prepared;out[2]=module->activeFrames;out[3]=module->downCalls;out[4]=module->inputCalls;out[5]=module->faults|(module->log.failed?16:0);out[6]=module->pulse?1:0;out[7]=module->desired?1:0;}
#ifdef _WIN32
extern "C" int __stdcall DllMain(HINSTANCE h,unsigned long reason,void *){if(reason==1)InitModule(h);else if(reason==0)ExitModule(h);return 1;}
#endif
