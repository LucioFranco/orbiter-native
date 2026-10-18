#include "Orbiter.h"
#include "Platform/DynamicLibrary.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==2,"graphics module required");Orbiter host;
        const std::filesystem::path path=argv[1];
        auto module=host.LoadModule(path.parent_path().string().c_str(),path.stem().string().c_str());
        auto gc=host.GetGraphicsClient();require(module && gc && gc->clbkInitialise(),"initialize graphics");
        using Preview=bool (*)(SURFHANDLE);using Read=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
        auto preview=reinterpret_cast<Preview>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_preview_surface"));
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));
        auto target=oapiCreateSurfaceEx(256,128,OAPISURFACE_RENDERTARGET),texture=oapiCreateSurfaceEx(1,1,OAPISURFACE_RENDERTARGET);
        require(target && texture && preview && read && preview(target),"ImGui target available");
        oapiColourFill(texture,0x00ff00);
        auto &io=ImGui::GetIO();io.DisplaySize=ImVec2(128,64);io.DisplayFramebufferScale=ImVec2(2,2);io.DeltaTime=1.0f/60;
        auto capture=[&]() {std::vector<BYTE> out(256*128*4);require(read(target,out.data(),out.size(),256*4),"read UI output");return out;};
        auto clear=[&]() {oapiColourFill(target,0x0000ff);};
        auto frame=[&](bool large) {
            clear();gc->clbkImGuiNewFrame();ImGui::NewFrame();
            auto bg=ImGui::GetBackgroundDrawList();bg->AddRectFilled(ImVec2(2,2),ImVec2(6,6),IM_COL32(255,255,0,255));
            auto list=ImGui::GetForegroundDrawList();
            list->PushClipRect(ImVec2(8,8),ImVec2(20,20),true);
            list->AddRectFilled(ImVec2(4,4),ImVec2(24,24),IM_COL32(255,0,0,128));list->PopClipRect();
            list->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
            list->AddImage(ImTextureRef(gc->clbkImGuiSurfaceTexture(texture)),ImVec2(30,8),ImVec2(50,20));
            list->AddText(ImVec2(4,36),IM_COL32_WHITE,"Apollo 11");
            if(large) {
                // More than 64K vertices in one list, retaining 16-bit ImDrawIdx.
                for(int i=0;i<18000;++i)list->AddRectFilled(ImVec2(-20,-20),ImVec2(-10,-10),IM_COL32_WHITE);
                list->AddRectFilled(ImVec2(90,8),ImVec2(100,20),IM_COL32(0,255,255,255));
            }
            ImGui::Render();gc->clbkImGuiRenderDrawData();return capture();
        };
        auto pixels=frame(false);
        auto pixel=[&](const std::vector<BYTE> &p,int x,int y) {const size_t i=(y*256+x)*4;return std::array<int,4>{p[i],p[i+1],p[i+2],p[i+3]};};
        require(pixel(pixels,20,20)==std::array<int,4>{128,0,127,255},"straight-alpha overlay preserves underlying scene");
        require(pixel(pixels,12,20)==std::array<int,4>{0,0,255,255} && pixel(pixels,41,20)==std::array<int,4>{0,0,255,255},"Retina clip bounds exclude geometry outside scissor");
        require(pixel(pixels,8,8)==std::array<int,4>{255,255,0,255},"multiple draw lists preserve ordering and vertex offsets");
        require(pixel(pixels,70,20)==std::array<int,4>{0,255,0,255},"Orbiter surface used as ImGui image");
        size_t letters=0;for(int y=72;y<120;++y)for(int x=0;x<200;++x)if(pixel(pixels,x,y)[0]>30)++letters;
        require(letters>100,"ImGui atlas produces readable text pixels");
        auto reject=[&](const char *why) {
            bool failed=false;try {gc->clbkImGuiRenderDrawData();}catch(const std::runtime_error &) {failed=true;}
            require(failed && capture()==pixels,why);
        };
        auto firstList=ImGui::GetDrawData()->CmdLists[0];
        const auto originalIndex=firstList->IdxBuffer[0];firstList->IdxBuffer[0]=ImDrawIdx(65535);
        reject("invalid indices rejected before modifying the frame");firstList->IdxBuffer[0]=originalIndex;
        auto &firstCommand=firstList->CmdBuffer[0];const auto originalTexture=firstCommand.TexRef;
        firstCommand.TexRef=ImTextureRef(ImTextureID(0x7fffffffffffffffULL));
        reject("unknown textures rejected before modifying the frame");
        firstCommand.TexRef=ImTextureRef(gc->clbkImGuiSurfaceTexture(target));
        reject("render-target feedback rejected");firstCommand.TexRef=originalTexture;
        const auto originalClip=firstCommand.ClipRect;firstCommand.ClipRect.x=std::numeric_limits<float>::quiet_NaN();
        reject("nonfinite clipping rejected");firstCommand.ClipRect=originalClip;
        // A viewport origin offset must cancel in both vertex and clip transforms.
        auto data=ImGui::GetDrawData();data->DisplayPos=ImVec2(31,17);
        for(auto list:data->CmdLists) {
            for(auto &v:list->VtxBuffer) {v.pos.x+=31;v.pos.y+=17;}
            for(auto &c:list->CmdBuffer) {c.ClipRect.x+=31;c.ClipRect.z+=31;c.ClipRect.y+=17;c.ClipRect.w+=17;}
        }
        clear();gc->clbkImGuiRenderDrawData();require(capture()==pixels,"nonzero display origin preserves geometry and clips");
        pixels=frame(true);bool offset=false;
        for(auto list:ImGui::GetDrawData()->CmdLists)for(const auto &cmd:list->CmdBuffer)offset|=cmd.VtxOffset>0;
        require(offset && pixel(pixels,190,20)==std::array<int,4>{0,255,255,255},"large 16-bit draw list renders commands beyond 64K vertices");
        // Force new font-size glyphs into the dynamic atlas on a later frame.
        clear();gc->clbkImGuiNewFrame();ImGui::NewFrame();ImGui::PushFont(nullptr,30);
        ImGui::GetForegroundDrawList()->AddText(ImVec2(2,2),IM_COL32_WHITE,"Moon");ImGui::PopFont();
        ImGui::Render();gc->clbkImGuiRenderDrawData();auto largeFont=capture();
        require(largeFont!=pixels,"dynamic font atlas changes render");
        for(auto tex:ImGui::GetPlatformIO().Textures)require(tex->Status==ImTextureStatus_OK,"font atlas requests consumed");
        gc->clbkImGuiShutdown();gc->clbkImGuiInit();
        const auto restarted=frame(false);require(restarted==frame(false),"renderer restart recreates font textures consistently");
        clear();gc->clbkImGuiNewFrame();ImGui::NewFrame();ImGui::Render();gc->clbkImGuiRenderDrawData();
        require(pixel(capture(),20,20)==std::array<int,4>{0,0,255,255},"empty UI preserves scene");
        oapiDestroySurface(texture);oapiDestroySurface(target);
        std::cout<<"ImGui batches, alpha, clipping, Retina scale, display origin, surfaces, large lists and dynamic fonts verified\n";
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
