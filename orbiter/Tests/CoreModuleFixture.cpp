#include "CoreState.h"
#include "Element.h"
#include <string>

namespace {
int initCount = 0;
int pauseCount = 0;
bool paused = false;
int startCount = 0, preCount = 0, postCount = 0;
std::string callbacks;
HINSTANCE initializedHandle = nullptr;
void (*exitObserver)() = nullptr;
}

#ifdef LEGACY_MODULE_CALLBACKS
#define FIXTURE_INIT opcDLLInit
#define FIXTURE_EXIT opcDLLExit
#else
#define FIXTURE_INIT InitModule
#define FIXTURE_EXIT ExitModule
#endif

DLLCLBK void FIXTURE_INIT(HINSTANCE handle) { ++initCount; initializedHandle = handle; }
DLLCLBK void FIXTURE_EXIT(HINSTANCE) { if (exitObserver) exitObserver(); }
DLLCLBK int InitializationCount() { return initCount; }
DLLCLBK HINSTANCE InitializedHandle() { return initializedHandle; }
DLLCLBK void SetExitObserver(void (*observer)()) { exitObserver = observer; }

DLLCLBK TimeData *SharedClock() { return &td; }
DLLCLBK void opcPause(bool value) {++pauseCount;paused=value;callbacks += value ? "pause;" : "resume;";}
DLLCLBK int PauseCount() {return pauseCount;}
DLLCLBK bool LastPause() {return paused;}
DLLCLBK void opcOpenRenderViewport(HWND,DWORD,DWORD,BOOL) {++startCount;callbacks += "start;";}
DLLCLBK void opcPreStep(double,double,double) {++preCount;callbacks += "pre;";}
DLLCLBK void opcPostStep(double,double,double) {++postCount;callbacks += "post;";}
DLLCLBK int StartCount() {return startCount;}
DLLCLBK int PreCount() {return preCount;}
DLLCLBK int PostCount() {return postCount;}
DLLCLBK const char *CallbackOrder() {return callbacks.c_str();}

DLLCLBK double OrbitalRadius(double elapsed)
{
    Elements orbit(7000000.0, 0.1, 0.2, 0.3, 0.4, 0.4, td.MJD_ref);
    orbit.Setup(10000.0, 5.972e24, td.MJD_ref);
    return orbit.Pos(elapsed).length();
}
