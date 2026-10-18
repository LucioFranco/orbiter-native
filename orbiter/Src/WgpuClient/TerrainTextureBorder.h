#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

// Surround a tile with half a tile of its eight neighbours. Copy BC1 blocks
// directly, preserving the downloaded base imagery and power-of-two mip sizes.
// Inputs are north-to-south rows, west-to-east columns; centre is element 4.
// Missing/incompatible neighbours leave the original texture path available.
inline std::vector<uint8_t> BorderTerrainTexture(const std::array<std::vector<uint8_t>,9> &tiles) {
    auto word=[](const auto &bytes,size_t offset) {
        uint32_t value=0;for(unsigned k=0;k<4;++k)value|=uint32_t(bytes[offset+k])<<(8*k);return value;
    };
    if(tiles[4].size()<128)return {};
    const auto size=word(tiles[4],16);
    if(size<8 || size>2048 || (size&(size-1)))return {};
    const unsigned blocks=size/4;
    for(const auto &tile:tiles) {
        if(tile.size()<128+size_t(blocks)*blocks*8 || std::memcmp(tile.data(),"DDS ",4) ||
           word(tile,4)!=124 || std::memcmp(tile.data()+84,"DXT1",4) ||
           word(tile,12)!=size || word(tile,16)!=size || word(tile,28)>1)return {};
    }
    std::vector<uint8_t> out(128+size_t(blocks)*blocks*32);
    std::memcpy(out.data(),tiles[4].data(),128);
    auto put=[&](size_t offset,uint32_t value) {for(unsigned k=0;k<4;++k)out[offset+k]=uint8_t(value>>(8*k));};
    put(12,size*2);put(16,size*2);put(20,uint32_t(out.size()-128));put(28,1);
    put(8,(word(out,8)|0x80000u)&~0x20000u);put(108,0x1000);
    for(unsigned y=0;y<blocks*2;++y)for(unsigned x=0;x<blocks*2;++x) {
        const unsigned sx=x+blocks/2,sy=y+blocks/2;
        const auto &tile=tiles[(sy/blocks)*3+sx/blocks];
        std::memcpy(out.data()+128+(size_t(y)*blocks*2+x)*8,
                    tile.data()+128+(size_t(sy%blocks)*blocks+sx%blocks)*8,8);
    }
    return out;
}
