#include "StarCatalog.h"
#include <fstream>
#include <iostream>
#include <sstream>
static void require(bool value,const char *why) {if(!value)throw std::runtime_error(why);}
static std::string record(float longitude,float latitude,float magnitude,uint16_t spectrum) {
    std::string bytes;
    for(float value:{longitude,latitude,magnitude}) {
        uint32_t bits;std::memcpy(&bits,&value,4);
        for(int shift=0;shift<32;shift+=8)bytes+=char(bits>>shift);
    }
    bytes+=char(spectrum);bytes+=char(spectrum>>8);return bytes;
}
int main(int argc,char **argv) {
    try {
        require(argc==2,"catalog path required");StarRenderPrm prm{2,8,0.1,true};
        std::istringstream input(record(0,0,1,0)+record(float(PI/2),0,4,60)+record(0,float(PI/2),7,30)+record(0,0,8,30));
        auto stars=orbiter::graphics::LoadStarCatalog(input,prm);
        require(stars.size()==3,"magnitude cutoff");
        require(stars[0].direction[0]>0.999 && stars[1].direction[2]>0.999 && stars[2].direction[1]>0.999,"catalog ecliptic axes");
        require(stars[0].color[2]>stars[0].color[0] && stars[1].color[0]>stars[1].color[2],"spectral blue and red colours");
        require(stars[1].color[0]>stars[2].color[0],"magnitude brightness ordering");
        bool rejected=false;std::istringstream broken(record(0,0,1,0)+"x");
        try {orbiter::graphics::LoadStarCatalog(broken,prm);}catch(const std::runtime_error &) {rejected=true;}
        require(rejected,"truncated catalog rejected");
        std::ifstream actual(argv[1],std::ios::binary);require(bool(actual),"open Orbiter catalog");
        stars=orbiter::graphics::LoadStarCatalog(actual,prm);
        require(stars.size()==41057,"real catalog magnitude selection");
        for(const auto &star:stars) {
            double length=0;for(float value:star.direction)length+=value*value;
            require(std::abs(length-1)<1e-6,"catalog directions normalized");
        }
        std::cout<<stars.size()<<" Orbiter stars decoded\n";
    }catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
