#pragma once
#include "OrbiterAPI.h"
class Instrument;
class Vessel;
namespace orbiter {
void NativeMFDInit();
void NativeMFDShutdown();
void NativeMFDFrameEnd();
void NativeMFDForget(Vessel *vessel);
bool NativeMFDOpen(int id,int mode);
Instrument *NativeMFDGet(int id);
void NativeMFDToggle();
void NativeMFDPower(int id);
bool NativeMFDVisible();
void NativeMFDDraw();
void NativeMFDUpdateCockpit();
void NativeDialogsDraw();
bool NativeDialogsActive();
void NativeDialogsReset();
}
#include <iosfwd>
#include <string>
namespace orbiter {
void NativeMFDReleaseRegistry();
void NativeMFDRead(const std::string &path);
void NativeMFDWrite(std::ostream &out);
}
