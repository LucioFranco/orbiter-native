#include "WgpuBridge.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(std::string(why)+": "+orb_gpu_error());}
int main() {
    try {
        std::unique_ptr<OrbGpu,decltype(&orb_gpu_destroy)> gpu(orb_gpu_create(),orb_gpu_destroy);
        require(bool(gpu),"GPU device");
        OrbMeshVertex vertices[3]{};
        for(auto &v:vertices) {v.position[0]=4;v.normal[2]=-1;v.color[2]=v.color[3]=1;}
        vertices[1].position[1]=1;vertices[2].position[0]=5;
        const uint32_t indices[]={0,1,2};
        auto mesh=orb_gpu_mesh_create(gpu.get(),vertices,3,indices,3);require(mesh,"mesh for sky scene");
        const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        float projection[16]{};projection[0]=projection[5]=projection[11]=1;
        const OrbStar stars[]={{{0,0,1},{1,0,0},3},{{float(1/std::sqrt(5.0)),0,float(2/std::sqrt(5.0))},{0,1,0},3},{{0,0,-1},{0,0,1},3}};
        require(orb_gpu_stars_upload(gpu.get(),stars,3),"star upload");
        require(orb_gpu_stars_camera(gpu.get(),projection),"sky projection");
        auto target=orb_gpu_surface_create(gpu.get(),64,64);
        auto render=[&](uint64_t surface,unsigned size) {
            require(orb_gpu_mesh_render(gpu.get(),surface,mesh,identity),"render stars and geometry");
            std::vector<uint8_t> pixels(size*size*4);
            require(orb_gpu_surface_read(gpu.get(),surface,pixels.data(),pixels.size(),size*4),"read sky pixels");return pixels;
        };
        const auto sky=render(target,64);
        const size_t center=(31*64+31)*4,side=(31*64+47)*4;
        require(sky[center]>100 && sky[center+1]==0 && sky[center+2]==0,"forward star visible; rear star clipped");
        require(sky[side+1]>100 && sky[side]==0,"off-axis catalog direction projected correctly");
        projection[12]=1e10f;projection[13]=-1e10f;projection[14]=1e10f;
        require(orb_gpu_stars_camera(gpu.get(),projection),"translated camera");
        require(render(target,64)==sky,"astronomical camera translation cannot move stars");
        auto large=orb_gpu_surface_create(gpu.get(),128,128);const auto resized=render(large,128);
        auto lit=[](const auto &pixels) {size_t n=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]||pixels[i+1]||pixels[i+2])++n;return n;};
        require(lit(resized)==lit(sky),"star size stays fixed in pixels after resize");
        projection[0]=-1;projection[11]=-1;
        require(orb_gpu_stars_camera(gpu.get(),projection),"rotate sky camera");
        const auto rotated=render(target,64);
        require(rotated[center]==0 && rotated[center+2]>100,"rotation brings rear star into view");
        projection[0]=projection[11]=1;require(orb_gpu_stars_camera(gpu.get(),projection),"restore camera");
        const float xy[3][2]={{-1,-1},{1,-1},{0,1}};
        for(int i=0;i<3;++i) {vertices[i].position[0]=xy[i][0];vertices[i].position[1]=xy[i][1];vertices[i].position[2]=1e-7f;}
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,3),"foreground geometry");
        const auto occluded=render(target,64);
        require(occluded[center]==0 && occluded[center+2]>128,"even distant geometry occludes infinite sky");
        OrbStar invalid=stars[0];invalid.direction[0]=10;
        require(!orb_gpu_stars_upload(gpu.get(),&invalid,1),"invalid direction rejected");
        for(auto &v:vertices)v.position[0]+=4;
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,3),"move occluder away");
        require(render(target,64)==sky,"rejected upload preserves prior catalog");
        require(orb_gpu_stars_upload(gpu.get(),nullptr,0),"clear catalog");
        require(lit(render(target,64))==0,"cleared sky is black");
        std::cout<<"Star projection, translation invariance, resize, clipping, and reversed-Z occlusion verified\n";
    }catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
