#include "Orbiter.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "NativeFontData.h"
namespace orbiter {
void CreateNativeImGuiContext() {
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    // Same bundled family as Sketchpad's Arial/Sans map, independent of OS fonts.
    const auto &font=graphics::fonts::Roboto_Regular;
    ImFontConfig config;config.FontDataOwnedByAtlas=false;
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(font),sizeof(font),18,&config);
    ImGui::StyleColorsDark();
}
}

// Event coordinates can be newer than ImGui's previous-frame capture flags.
// Consult the displayed windows, including MFDs and selection popups.
bool Orbiter::NativeUIContains(float x,float y) const {
    auto context=ImGui::GetCurrentContext();if(!context)return false;
    for(auto window:context->Windows)
        if(window->Active && !window->Hidden && !window->IsFallbackWindow &&
           !(window->Flags & ImGuiWindowFlags_NoMouseInputs) && window->OuterRectClipped.Contains(ImVec2(x,y)))return true;
    return false;
}
