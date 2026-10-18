#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <istream>
#include <stdexcept>
#include <vector>
#include <utility>
// Orbiter legacy .tex files concatenate DDS images. The first three are
// complete equirectangular spheres; later images are subdivided patches.
inline std::vector<uint8_t> ReadSkyDDS(std::istream &input) {
    std::vector<uint8_t> bytes(128);
    if(!input.read(reinterpret_cast<char *>(bytes.data()),128))throw std::runtime_error("Truncated sky DDS header");
    auto word=[&](size_t offset) {uint32_t v=0;for(unsigned k=0;k<4;++k)v|=uint32_t(bytes[offset+k])<<(8*k);return v;};
    const auto width=word(16),height=word(12),mips=std::max(1u,word(28));
    if(std::memcmp(bytes.data(),"DDS ",4) || word(4)!=124 || !width || !height || width>8192 || height>8192 || mips>14)
        throw std::runtime_error("Invalid sky DDS dimensions");
    unsigned block=0;
    if(!std::memcmp(bytes.data()+84,"DXT1",4))block=8;
    else if(!std::memcmp(bytes.data()+84,"DXT3",4) || !std::memcmp(bytes.data()+84,"DXT5",4))block=16;
    else throw std::runtime_error("Unsupported sky DDS compression");
    size_t size=0;unsigned w=width,h=height;
    for(unsigned mip=0;mip<mips;++mip) {size+=size_t((w+3)/4)*((h+3)/4)*block;w=std::max(1u,w/2);h=std::max(1u,h/2);}
    bytes.resize(128+size);
    if(!input.read(reinterpret_cast<char *>(bytes.data()+128),size))throw std::runtime_error("Truncated sky DDS pixels");
    return bytes;
}
inline std::vector<uint8_t> ReadSkyOverview(std::istream &input) {
    ReadSkyDDS(input);ReadSkyDDS(input);return ReadSkyDDS(input);
}


struct SkyTexture {std::vector<uint8_t> dds;unsigned level=3;};
// Pack the highest complete legacy level into a block-compressed atlas. Loading
// is one-time; camera motion only updates the ray basis, never tile uploads.
inline SkyTexture ReadSkyTexture(std::istream &input) {
    std::vector<std::vector<uint8_t>> images;
    while(input.peek()!=std::char_traits<char>::eof() && images.size()<501)images.push_back(ReadSkyDDS(input));
    if(images.size()<3)throw std::runtime_error("Missing sky overview");
    const unsigned offsets[]={0,1,2,3,5,13,37,137,501};
    unsigned level=8;while(level>4 && images.size()<offsets[level])--level;
    if(level<5)return {std::move(images[2]),3};
    const auto first=offsets[level-1],count=offsets[level]-first;
    auto &header=images[first];
    auto word=[](const std::vector<uint8_t>& b,unsigned o) {uint32_t v=0;for(unsigned k=0;k<4;++k)v|=uint32_t(b[o+k])<<(8*k);return v;};
    const unsigned block=std::memcmp(header.data()+84,"DXT1",4)==0?8:16;
    const unsigned columns=32,rows=(count+columns-1)/columns;
    const unsigned width=columns*256,height=rows*256;
    std::vector<uint8_t> atlas(128+size_t(width/4)*(height/4)*block);
    std::copy_n(header.begin(),128,atlas.begin());
    auto put=[&](unsigned o,uint32_t v){for(unsigned k=0;k<4;++k)atlas[o+k]=uint8_t(v>>(8*k));};
    put(12,height);put(16,width);put(20,uint32_t(atlas.size()-128));put(28,1);put(108,0x1000);
    for(unsigned i=0;i<count;++i) {
        const auto &tile=images[first+i];
        if(word(tile,12)!=256 || word(tile,16)!=256 || std::memcmp(tile.data()+84,header.data()+84,4))
            throw std::runtime_error("Inconsistent sky tile format");
        for(unsigned y=0;y<64;++y) {
            const size_t dst=128+(size_t(i/columns*64+y)*(width/4)+(i%columns)*64)*block;
            std::copy_n(tile.begin()+128+size_t(y)*64*block,64*block,atlas.begin()+dst);
        }
    }
    return {std::move(atlas),level};
}
