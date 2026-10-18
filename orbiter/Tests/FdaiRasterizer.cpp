#include "FdaiRasterizer.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        constexpr double pi=3.14159265358979323846;
        orbiter::platform::ImageRGBA texture{1024,512,std::vector<unsigned char>(1024*512*4)};
        for(unsigned y=0;y<texture.height;++y)for(unsigned x=0;x<texture.width;++x) {
            auto p=&texture.pixels[(y*texture.width+x)*4];p[0]=x*255/(texture.width-1);p[1]=y*255/(texture.height-1);p[2]=80;p[3]=255;
        }
        auto center=[&](double yaw,double pitch,int red,int green){
            const auto pixels=nassp::FdaiRasterizer::Render(texture,0,yaw,pitch);
            const auto *p=&pixels[(90*180+90)*4];
            require(std::abs(int(p[0])-red)<3 && std::abs(int(p[1])-green)<3,"GLU longitude/latitude orientation at center");
            require(pixels[0]==0 && pixels[1]==0 && pixels[2]==0 && pixels[3]==255,"pixels outside projected sphere are black");
        };
        center(0,0,128,128);center(0,pi/2,191,128);center(0,-pi/2,64,128);
        const auto north=nassp::FdaiRasterizer::Render(texture,0,pi/2,0);
        require(north[(90*180+90)*4+1]<3,"positive yaw exposes north pole");
        const auto south=nassp::FdaiRasterizer::Render(texture,0,-pi/2,0);
        require(south[(90*180+90)*4+1]>252,"negative yaw exposes south pole");
        auto base=nassp::FdaiRasterizer::Render(texture,.3,.4,.5);
        auto wrapped=nassp::FdaiRasterizer::Render(texture,.3+2*pi,.4-2*pi,.5+2*pi);
        require(base==wrapped,"attitude wraps at a full revolution");
        bool rejected=false;try{nassp::FdaiRasterizer::Render(texture,0,0,std::numeric_limits<double>::quiet_NaN());}catch(const std::invalid_argument &){rejected=true;}
        require(rejected,"reject nonfinite attitude");
        require(argc==3,"real CSM and LM texture paths required");
        auto csm=orbiter::platform::LoadImageRGBA(argv[1]),lm=orbiter::platform::LoadImageRGBA(argv[2]);
        require(csm.width==1024 && csm.height==512 && lm.width==1024 && lm.height==512,"decode original BMP-signature .dds assets");
        auto csmBall=nassp::FdaiRasterizer::Render(csm,0,0,0),lmBall=nassp::FdaiRasterizer::Render(lm,0,0,0);
        require(csmBall!=lmBall && csmBall!=nassp::FdaiRasterizer::Render(csm,0,.5,0),"real ball assets and attitude produce distinct images");
        std::cout<<"CPU FDAI sphere projection, texture axes, poles, wrap and real assets verified\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
