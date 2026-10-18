#include "ZTreeMgr.h"
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <future>
static void require(bool ok,const char *why) { if (!ok) throw std::runtime_error(why); }
using Bytes=std::vector<BYTE>;
static void put32(Bytes &out,std::size_t at,std::uint32_t value) { for(unsigned i=0;i<4;++i)out.at(at+i)=BYTE(value>>(8*i)); }
static void put64(Bytes &out,std::size_t at,std::uint64_t value) { for(unsigned i=0;i<8;++i)out.at(at+i)=BYTE(value>>(8*i)); }
static Bytes packed(const Bytes &data) {
    uLongf size=compressBound(data.size()); Bytes out(size);
    require(compress2(out.data(),&size,data.data(),data.size(),Z_BEST_SPEED)==Z_OK,"compress archive fixture");
    out.resize(size);return out;
}
int main(int argc,char **argv) {
    try {
        require(argc==2,"scratch directory required");
        const auto root=std::filesystem::path(argv[1]);
        std::filesystem::create_directories(root/"MoOn/aRcHiVe");
        const auto path=root/"MoOn/aRcHiVe/sUrF.TrEe";
        const auto planet=(root/"moon").string();
        const Bytes payloads[]={Bytes{'D','D','S',' ',1,2,3},Bytes{'D','D','S',' ',4,5},Bytes{'E','L','E',1,6,7,8,9}};
        const auto archive=[&](bool compressed) {
            // Two hemisphere roots; first root has a descendant at level 5.
            Bytes out(48+3*32,0);
            out[0]='T';out[1]='X';out[2]=1;
            put32(out,4,48);put32(out,8,compressed?1:0);put32(out,12,out.size());put32(out,24,3);
            for(unsigned offset=28;offset<48;offset+=4)put32(out,offset,0xffffffffu);
            put32(out,40,0);put32(out,44,2);
            std::uint64_t position=0;
            for(unsigned i=0;i<3;++i) {
                const auto bytes=compressed?packed(payloads[i]):payloads[i];
                put64(out,48+i*32,position);put32(out,56+i*32,payloads[i].size());
                for(unsigned child=0;child<4;++child)put32(out,60+i*32+child*4,0xffffffffu);
                if(i==0)put32(out,60+3*4,1); // south-east child
                out.insert(out.end(),bytes.begin(),bytes.end());position+=bytes.size();
            }
            put64(out,16,position);return out;
        };
        auto write=[&](const Bytes &data) {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char *>(data.data()),data.size());require(bool(file),"write tile archive");};
        auto read=[&](ZTreeMgr &tree,int level,int lat,int lng,const Bytes &expected) {
            BYTE *data=nullptr;const auto count=tree.ReadData(level,lat,lng,&data);
            require(count==expected.size() && data,"tile payload returned");
            const Bytes got(data,data+count);tree.ReleaseData(data);require(got==expected,"tile payload preserved byte-for-byte");
        };
        for(bool compressed:{true,false}) {
            write(archive(compressed));std::unique_ptr<ZTreeMgr> tree(ZTreeMgr::CreateFromFile(planet.c_str(),ZTreeMgr::LAYER_SURF));
            require(tree && tree->TOC().size()==3,"portable archive open");
            read(*tree,4,0,0,payloads[0]);read(*tree,5,1,1,payloads[1]);read(*tree,4,0,1,payloads[2]);
            require(tree->Idx(1,0,0)==DWORD(-1) && tree->Idx(5,0,0)==DWORD(-1),"missing roots and branches");
            for(const auto &coords:std::vector<std::vector<int>>{{0,0,0},{31,0,0},{4,1,0},{4,0,2},{5,-1,0},{5,0,4},{1,0,1}})
                require(tree->Idx(coords[0],coords[1],coords[2])==DWORD(-1),"invalid tile coordinates rejected");
            BYTE *data=reinterpret_cast<BYTE *>(1);
            require(!tree->ReadData(100,&data) && !data && !tree->ReadData(0,nullptr),"invalid read clears output");
            data=reinterpret_cast<BYTE *>(1);
            require(!tree->ReadData(5,-1,0,&data) && !data,"invalid coordinates clear read output");
            auto a=std::async(std::launch::async,[&](){for(int i=0;i<100;++i)read(*tree,4,0,0,payloads[0]);});
            auto b=std::async(std::launch::async,[&](){for(int i=0;i<100;++i)read(*tree,5,1,1,payloads[1]);});a.get();b.get();
        }
        const auto valid=archive(true);
        // Sparse archives may contain an empty parent with populated descendants.
        auto sparse=valid;
        const auto first=packed(payloads[0]).size(), second=packed(payloads[1]).size();
        sparse.erase(sparse.begin()+144,sparse.begin()+144+first);
        put32(sparse,56,0);put64(sparse,80,0);put64(sparse,112,second);put64(sparse,16,sparse.size()-144);
        write(sparse);
        {
            std::unique_ptr<ZTreeMgr> tree(ZTreeMgr::CreateFromFile(planet.c_str(),ZTreeMgr::LAYER_SURF));
            require(bool(tree),"sparse archive opens");
            BYTE *data=reinterpret_cast<BYTE *>(1);
            require(!tree->ReadData(4,0,0,&data) && !data,"empty parent has no payload");
            read(*tree,5,1,1,payloads[1]);
        }
        // Exercise a real 64-bit file seek without allocating a multi-GB buffer.
        {
            const std::uint64_t offset=(std::uint64_t(1)<<32)+17;
            const auto data=packed(payloads[0]);
            Bytes header(80,0); header[0]='T';header[1]='X';header[2]=1;
            put32(header,4,48);put32(header,8,1);put32(header,12,80);put64(header,16,offset+data.size());put32(header,24,1);
            for(unsigned at=28;at<48;at+=4)put32(header,at,0xffffffffu);
            put32(header,40,0);put64(header,48,offset);put32(header,56,payloads[0].size());
            for(unsigned at=60;at<76;at+=4)put32(header,at,0xffffffffu);
            {
                std::ofstream file(path,std::ios::binary);
                file.write(reinterpret_cast<const char *>(header.data()),header.size());
                file.seekp(static_cast<std::streamoff>(80+offset));
                file.write(reinterpret_cast<const char *>(data.data()),data.size());
                require(bool(file),"write sparse archive beyond 4 GiB");
            }
            std::unique_ptr<ZTreeMgr> tree(ZTreeMgr::CreateFromFile(planet.c_str(),ZTreeMgr::LAYER_SURF));
            require(bool(tree),"large archive opens without truncating offsets");
            read(*tree,4,0,0,payloads[0]);
        }
        auto reject=[&](Bytes data) {write(data);std::unique_ptr<ZTreeMgr> tree(ZTreeMgr::CreateFromFile(planet.c_str(),ZTreeMgr::LAYER_SURF));require(!tree,"malformed archive rejected");};
        for(std::size_t length:{std::size_t(3),std::size_t(47),std::size_t(70),valid.size()-1}) reject(Bytes(valid.begin(),valid.begin()+length));
        auto bad=valid;put32(bad,24,0xffffffffu);reject(bad);
        bad=valid;put32(bad,12,48);reject(bad);
        bad=valid;put32(bad,40,3);reject(bad);
        bad=valid;put32(bad,60,9);reject(bad);
        bad=valid;put64(bad,48,0xffffffffffffffffull);reject(bad);
        bad=valid;put64(bad,80,0);put64(bad,112,0);reject(bad);
        bad=valid;put32(bad,56,0xffffffffu);reject(bad);
        bad=valid;put32(bad,8,2);reject(bad);
        bad=valid;put32(bad,56,payloads[0].size()+1);write(bad);
        {
            std::unique_ptr<ZTreeMgr> tree(ZTreeMgr::CreateFromFile(planet.c_str(),ZTreeMgr::LAYER_SURF));
            require(bool(tree),"declared size checked during inflate");
            BYTE *data=reinterpret_cast<BYTE *>(1);
            require(!tree->ReadData(0,&data) && !data,"incomplete inflated payload rejected");
        }
        // Valid structure but invalid zlib data must never return an output buffer.
        bad=valid;bad[48+3*32]^=0xff;write(bad);
        std::unique_ptr<ZTreeMgr> broken(ZTreeMgr::CreateFromFile(planet.c_str(),ZTreeMgr::LAYER_SURF));
        require(bool(broken),"corrupt payload does not invalidate unrelated archive entries");
        BYTE *data=reinterpret_cast<BYTE *>(1);require(!broken->ReadData(0,&data) && !data,"bad deflate fails without stale output");
        read(*broken,4,0,1,payloads[2]);
        std::cout<<"Portable .tree structure, lookup, compression, malformed data and concurrent reads verified\n";
        return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
