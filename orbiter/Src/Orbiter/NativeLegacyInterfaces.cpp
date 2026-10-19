// Defaults for SDK interfaces whose Windows dialog host is absent natively.
#include "Orbitersdk.h"

LaunchpadItem::LaunchpadItem() : hItem(nullptr) {}
LaunchpadItem::~LaunchpadItem() = default;
char *LaunchpadItem::Name() { return nullptr; }
char *LaunchpadItem::Description() { return nullptr; }
bool LaunchpadItem::OpenDialog(HINSTANCE, HWND, int, DLGPROC) { return false; }
bool LaunchpadItem::clbkOpen(HWND) { return false; }
int LaunchpadItem::clbkWriteConfig() { return 0; }

// The native host has no legacy Help dialog. Preserve an honest failure for
// callers while retaining the original MFDAPI implementation and vtable.
DLLEXPORT bool oapiOpenHelp(HELPCONTEXT *) { return false; }
