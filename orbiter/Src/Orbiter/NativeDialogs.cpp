// Native host for the existing ImGui selection/input dialogs.
#include "NativeMFD.h"
#include "Select.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_extras.h"
#include <algorithm>
#include <vector>
extern Select *g_select;
extern InputBox *g_input;
namespace {std::vector<ImGuiDialog *> dialogs;}
DLLEXPORT void oapiOpenDialog(ImGuiDialog *dialog) {
    if(!dialog)return;
    if(std::find(dialogs.begin(),dialogs.end(),dialog)==dialogs.end())dialogs.push_back(dialog);
    dialog->Activate();
}
DLLEXPORT void oapiCloseDialog(ImGuiDialog *dialog) {
    dialogs.erase(std::remove(dialogs.begin(),dialogs.end(),dialog),dialogs.end());
}
ImGuiDialog::~ImGuiDialog() {oapiCloseDialog(this);}
bool ImGuiDialog::HandleHelpButton() {return false;}
void ImGuiDialog::Activate() {active=true;ImGui::SetWindowFocus(name.c_str());}
void ImGuiDialog::Display() {
    ImGui::SetNextWindowSize(ImVec2(defaultSize.width,defaultSize.height),ImGuiCond_FirstUseEver);
    if(ImGui::Begin(name.c_str(),&active))OnDraw();
    ImGui::End();if(!active)OnClose();
}
namespace ImGui {
bool BeginAnimatedPopup(const char *name,float) {return BeginPopup(name);}
void EndAnimatedPopup() {EndPopup();}
}
namespace orbiter {
bool NativeDialogsActive() {
    if((g_select && g_select->IsActive()) || (g_input && g_input->IsActive()))return true;
    for(auto dialog:dialogs)if(dialog->IsActive())return true;
    return false;
}
void NativeDialogsDraw() {
    if(g_select && g_select->IsActive())g_select->Display();
    if(g_input && g_input->IsActive())g_input->Display();
    const auto snapshot=dialogs;
    for(auto dialog:snapshot)if(std::find(dialogs.begin(),dialogs.end(),dialog)!=dialogs.end() && dialog->IsActive())dialog->Display();
}
void NativeDialogsReset() {
    // Select callbacks contain pointers to the instrument that opened them.
    // Close them before replacing a mode or deleting that vessel.
    if(ImGui::GetCurrentContext())ImGui::ClosePopupsOverWindow(nullptr,false);
    if(g_select) {g_select->Deactivate();g_select->opened=false;}else g_select=new Select();
    if(g_input) {g_input->Deactivate();g_input->opened=false;}else g_input=new InputBox();
}
}

DLLEXPORT void oapiOpenInputBox(char *title,bool (*callback)(void *,char *,void *),char *buffer,int length,void *context) {
    g_input->Open(title,buffer,length,reinterpret_cast<InputBox::Callbk>(callback),context);
}
DLLEXPORT void oapiOpenInputBoxEx(const char *title,bool (*enter)(void *,char *,void *),bool (*cancel)(void *,char *,void *),char *buffer,int length,void *context,DWORD flags) {
    g_input->OpenEx(title,buffer,length,reinterpret_cast<InputBox::Callbk>(enter),reinterpret_cast<InputBox::Callbk>(cancel),context,flags);
}
