#pragma once
#include "WgpuBridge.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
namespace orbiter::graphics {
inline void RenderImGuiDrawData(OrbGpu *gpu,uint64_t target,uint32_t width,uint32_t height,const ImDrawData *data) {
    if(!data || !data->Valid || data->TotalVtxCount==0 || width==0 || height==0)return;
    if(data->TotalVtxCount<0 || data->TotalIdxCount<0 ||
       !std::isfinite(data->DisplayPos.x) || !std::isfinite(data->DisplayPos.y) ||
       !std::isfinite(data->FramebufferScale.x) || !std::isfinite(data->FramebufferScale.y) ||
       data->FramebufferScale.x<=0 || data->FramebufferScale.y<=0)
        throw std::runtime_error("Invalid ImGui draw dimensions");
    std::vector<OrbUiVertex> vertices;std::vector<uint32_t> indices;std::vector<OrbUiCommand> commands;
    vertices.reserve(data->TotalVtxCount);indices.reserve(data->TotalIdxCount);
    for(const auto list:data->CmdLists) {
        const size_t base=vertices.size(),first=indices.size();
        for(const auto &v:list->VtxBuffer) {
            OrbUiVertex out{};
            out.position[0]=2*(v.pos.x-data->DisplayPos.x)*data->FramebufferScale.x/width-1;
            out.position[1]=1-2*(v.pos.y-data->DisplayPos.y)*data->FramebufferScale.y/height;
            out.uv[0]=v.uv.x;out.uv[1]=v.uv.y;
            out.color[0]=uint8_t(v.col>>IM_COL32_R_SHIFT);out.color[1]=uint8_t(v.col>>IM_COL32_G_SHIFT);
            out.color[2]=uint8_t(v.col>>IM_COL32_B_SHIFT);out.color[3]=uint8_t(v.col>>IM_COL32_A_SHIFT);
            vertices.push_back(out);
        }
        for(auto index:list->IdxBuffer)indices.push_back(index);
        for(const auto &cmd:list->CmdBuffer) {
            // There is no mutable graphics API state between batched commands.
            if(cmd.UserCallback==ImDrawCallback_ResetRenderState)continue;
            if(cmd.UserCallback)throw std::runtime_error("Custom ImGui render callbacks are not supported by the batched renderer");
            const float clip[]={
                (cmd.ClipRect.x-data->DisplayPos.x)*data->FramebufferScale.x,
                (cmd.ClipRect.y-data->DisplayPos.y)*data->FramebufferScale.y,
                (cmd.ClipRect.z-data->DisplayPos.x)*data->FramebufferScale.x,
                (cmd.ClipRect.w-data->DisplayPos.y)*data->FramebufferScale.y};
            for(float value:clip)if(!std::isfinite(value))throw std::runtime_error("Nonfinite ImGui clip rectangle");
            const auto x=uint32_t(std::floor(std::clamp(double(clip[0]),0.0,double(width))));
            const auto y=uint32_t(std::floor(std::clamp(double(clip[1]),0.0,double(height))));
            const auto right=uint32_t(std::ceil(std::clamp(double(clip[2]),0.0,double(width))));
            const auto bottom=uint32_t(std::ceil(std::clamp(double(clip[3]),0.0,double(height))));
            if(right<=x || bottom<=y || cmd.ElemCount==0)continue;
            if(base+cmd.VtxOffset>size_t(INT32_MAX) || first+cmd.IdxOffset>size_t(UINT32_MAX) ||
               uint64_t(cmd.IdxOffset)+cmd.ElemCount>uint64_t(list->IdxBuffer.Size))
                throw std::runtime_error("Invalid ImGui draw offsets");
            commands.push_back({uint64_t(cmd.GetTexID()),uint32_t(first+cmd.IdxOffset),cmd.ElemCount,
                int32_t(base+cmd.VtxOffset),{x,y,right-x,bottom-y},0});
        }
    }
    if(!orb_gpu_ui_render(gpu,target,vertices.data(),vertices.size(),indices.data(),indices.size(),commands.data(),commands.size()))
        throw std::runtime_error(orb_gpu_error());
}
}
