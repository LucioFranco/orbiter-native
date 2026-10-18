#include "TerrainTextureBorder.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main() {
    try {
        std::array<std::vector<uint8_t>,9> tiles;
        for(unsigned tile=0;tile<9;++tile) {
            auto &bytes=tiles[tile];bytes.resize(160);
            std::memcpy(bytes.data(),"DDS ",4);bytes[4]=124;bytes[12]=bytes[16]=8;bytes[28]=1;
            std::memcpy(bytes.data()+84,"DXT1",4);
            for(unsigned block=0;block<4;++block)
                std::fill_n(bytes.data()+128+block*8,8,uint8_t(tile*4+block));
        }
        const auto original=tiles;
        const auto padded=BorderTerrainTexture(tiles);
        require(padded.size()==256 && padded[12]==16 && padded[16]==16,"power-of-two bordered DDS");
        require(tiles==original,"downloaded source blocks remain unchanged");
        // Upper-left gutter is southeast block of northwest neighbour; centre
        // retains its four blocks, and lower-right is northwest of southeast.
        const unsigned expected[16]={3,6,7,10,13,16,17,20,15,18,19,22,25,28,29,32};
        for(unsigned block=0;block<16;++block)for(unsigned byte=0;byte<8;++byte)
            require(padded[128+block*8+byte]==expected[block],"neighbour block orientation and exact payload");
        tiles[0].clear();require(BorderTerrainTexture(tiles).empty(),"missing neighbour retains original path");
        tiles=original;tiles[3][28]=2;
        require(BorderTerrainTexture(tiles).empty(),"authored mip chains are not replaced");
        tiles=original;tiles[8].resize(159);
        require(BorderTerrainTexture(tiles).empty(),"truncated neighbour rejected");
        std::cout<<"Compressed terrain border orientation, preservation and fallback passed\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
