#include "WgpuBridge.h"
#include "TerrainTextureBorder.h"
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(std::string(message)+": "+orb_gpu_error());}
static void word(std::vector<uint8_t>& b,size_t at,uint32_t value) {for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(i*8));}
static std::vector<uint8_t> dds(unsigned kind,unsigned width=4,unsigned height=4) {
    const unsigned block=kind==1?8:16;
    std::vector<uint8_t> b(128+((width+3)/4)*((height+3)/4)*block);
    std::memcpy(b.data(),"DDS ",4);word(b,4,124);word(b,12,height);word(b,16,width);word(b,76,32);word(b,80,4);
    std::memcpy(b.data()+84,kind==1?"DXT1":kind==3?"DXT3":"DXT5",4);
    for(size_t i=128;i<b.size();i+=block) {
        auto color=i+(kind==1?0:8);
        b[color]=0; b[color+1]=0xf8; // RGB565 red, all selectors zero
        if(kind==3)for(unsigned j=0;j<8;++j)b[i+j]=255;
        if(kind==5)b[i]=255;
    }
    return b;
}
int main() {
    try {
        std::unique_ptr<OrbGpu,decltype(&orb_gpu_destroy)> gpu(orb_gpu_create(),orb_gpu_destroy);
        require(bool(gpu),"GPU device");
        const auto target=orb_gpu_surface_create(gpu.get(),4,4);
        for(unsigned kind:{1,3,5}) {
            auto bytes=dds(kind);
            // A full 4x4, 2x2, 1x1 BC mip chain tests padded tiny mip uploads.
            auto base=bytes;
            bytes.insert(bytes.end(),base.begin()+128,base.end());bytes.insert(bytes.end(),base.begin()+128,base.end());word(bytes,28,3);
            const auto texture=orb_gpu_dds_create(gpu.get(),bytes.data(),bytes.size());require(texture,"DDS upload");
            OrbDraw copy{1,texture,0,0,4,4,0,0,4,4,0,0,0,0};
            require(orb_gpu_queue_batch(gpu.get(),target,&copy,1),"DDS blit");
            std::array<uint8_t,64> pixels{};
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),16),"DDS readback");
            require(pixels[0]==255 && pixels[1]==0 && pixels[2]==0 && pixels[3]==255,"BC red pixel");
            require(!orb_gpu_surface_read(gpu.get(),texture,pixels.data(),pixels.size(),16),"sample-only read rejected");
            OrbMeshVertex vertices[3]{};
            const float positions[3][2]={{-1,-1},{3,-1},{-1,3}};
            for(int i=0;i<3;++i) {vertices[i].position[0]=positions[i][0];vertices[i].position[1]=positions[i][1];vertices[i].position[2]=0.5f;vertices[i].normal[2]=-1;vertices[i].uv[0]=vertices[i].uv[1]=0.5f;for(auto &channel:vertices[i].color)channel=1;}
            const uint32_t indices[]={0,1,2};auto mesh=orb_gpu_mesh_create(gpu.get(),vertices,3,indices,3);
            const OrbMeshGroup group{0,3,texture};require(orb_gpu_mesh_groups(gpu.get(),mesh,&group,1),"bind compressed mesh texture");
            require(orb_gpu_surface_make_writable(gpu.get(),texture),"promote DDS preserving handle");
            require(orb_gpu_surface_read(gpu.get(),texture,pixels.data(),pixels.size(),16),"read promoted texture");
            require(pixels[0]==255 && pixels[1]==0,"promotion preserves original pixels");
            const OrbFill fill{0,0,4,4,{0,255,0,255}};
            require(orb_gpu_surface_fill_batch(gpu.get(),texture,&fill,1),"write promoted texture");
            const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"render retained mesh binding");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),16),"promoted binding readback");
            require(pixels[0]==0 && pixels[1]>128 && pixels[2]==0,"mesh binding follows promoted texture");
            require(orb_gpu_mesh_release(gpu.get(),mesh),"release promoted mesh");
            bytes.resize(bytes.size()-1);require(!orb_gpu_dds_create(gpu.get(),bytes.data(),bytes.size()),"truncated mip rejected");
            require(orb_gpu_surface_release(gpu.get(),texture),"DDS release");
        }
        auto checker=dds(1,64,64);
        for(size_t at=128;at<checker.size();at+=8) {
            checker[at]=checker[at+1]=255;checker[at+2]=checker[at+3]=0;
            uint32_t selectors=0;for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)selectors|=((x+y)&1)<<(2*(y*4+x));
            word(checker,at+4,selectors);
        }
        const auto mipTexture=orb_gpu_dds_create_mipped(gpu.get(),checker.data(),checker.size());
        require(mipTexture,"generate missing BC1 terrain mip chain");
        OrbMeshVertex mipVertices[3]{};const float mipXY[3][2]={{-1,-1},{3,-1},{-1,3}};
        for(int i=0;i<3;++i) {
            mipVertices[i].position[0]=mipXY[i][0];mipVertices[i].position[1]=mipXY[i][1];mipVertices[i].position[2]=0.5f;
            mipVertices[i].normal[2]=-1;
            mipVertices[i].uv[0]=(mipXY[i][0]+1)*16+0.1f;mipVertices[i].uv[1]=(mipXY[i][1]+1)*16+0.1f;
            for(auto &c:mipVertices[i].color)c=1;
        }
        const uint32_t mipIndices[]={0,1,2};const auto mipMesh=orb_gpu_mesh_create(gpu.get(),mipVertices,3,mipIndices,3);
        const OrbMeshGroup mipGroup{0,3,mipTexture};require(orb_gpu_mesh_groups(gpu.get(),mipMesh,&mipGroup,1),"bind generated terrain mips");
        OrbMeshDraw mipDraw{};mipDraw.mesh=mipMesh;mipDraw.sunlight[3]=1;
        mipDraw.mvp[0]=mipDraw.mvp[5]=mipDraw.mvp[10]=mipDraw.mvp[15]=1;
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&mipDraw,1),"minified checker render");
        std::array<uint8_t,64> mipPixels{};
        require(orb_gpu_surface_read(gpu.get(),target,mipPixels.data(),mipPixels.size(),16),"generated mip readback");
        for(size_t i=0;i<mipPixels.size();i+=4)require(mipPixels[i]>=120 && mipPixels[i]<=136 && mipPixels[i+1]>=120 && mipPixels[i+1]<=136 && mipPixels[i+2]>=120 && mipPixels[i+2]<=136 && mipPixels[i+3]==255,"minified checker averages to opaque grey");
        require(orb_gpu_surface_make_writable(gpu.get(),mipTexture),"inspect original mip base");
        std::vector<uint8_t> basePixels(64*64*4);
        require(orb_gpu_surface_read(gpu.get(),mipTexture,basePixels.data(),basePixels.size(),64*4),"read unchanged original base");
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
            const auto i=(y*64+x)*4;const auto expected=((x+y)&1)?0:255;
            require(basePixels[i]==expected && basePixels[i+1]==expected && basePixels[i+2]==expected && basePixels[i+3]==255,"mip generation preserves original BC1 checker pixels");
        }
        auto authored=checker;
        for(unsigned dimension=32;dimension;dimension/=2) {
            auto blue=dds(1,dimension,dimension);
            for(size_t i=128;i<blue.size();i+=8){blue[i]=31;blue[i+1]=0;}
            authored.insert(authored.end(),blue.begin()+128,blue.end());
        }
        word(authored,28,7);
        const auto authoredTexture=orb_gpu_dds_create_mipped(gpu.get(),authored.data(),authored.size());
        require(authoredTexture,"load authored mip chain");
        const OrbMeshGroup authoredGroup{0,3,authoredTexture};
        require(orb_gpu_mesh_groups(gpu.get(),mipMesh,&authoredGroup,1),"bind authored mip chain");
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&mipDraw,1),"render authored mips");
        require(orb_gpu_surface_read(gpu.get(),target,mipPixels.data(),mipPixels.size(),16),"authored mip readback");
        require(mipPixels[0]==0 && mipPixels[1]==0 && mipPixels[2]==255,"supplied blue mips are not regenerated from checker base");
        require(orb_gpu_surface_release(gpu.get(),authoredTexture),"release authored mip texture");
        for(unsigned axis=0;axis<2;++axis)for(unsigned side=0;side<2;++side) {
            std::array<std::vector<uint8_t>,9> neighbors;
            for(unsigned i=0;i<9;++i) {
                auto &bytes=neighbors[i];bytes=dds(1,8,8);
                const bool white=(axis?i/3:i%3)+side>=2;
                for(size_t at=128;at<bytes.size();at+=8)bytes[at]=bytes[at+1]=white?255:0;
            }
            const auto bordered=BorderTerrainTexture(neighbors);
            require(!bordered.empty(),"create compressed neighbour border");
            const auto texture=orb_gpu_dds_create_mipped(gpu.get(),bordered.data(),bordered.size());
            require(texture,"upload bordered terrain and generate mips");
            const OrbMeshGroup borderedGroup{0,3,texture,0,1};
            require(orb_gpu_mesh_groups(gpu.get(),mipMesh,&borderedGroup,1),"bind bordered terrain");
            for(auto &vertex:mipVertices) {
                vertex.uv[0]=vertex.uv[1]=.5f;vertex.uv[axis]=side?.25f:.75f;
            }
            require(orb_gpu_mesh_update(gpu.get(),mipMesh,mipVertices,3),"sample exact common tile edge");
            require(orb_gpu_mesh_render_scene(gpu.get(),target,&mipDraw,1),"render neighbour-filtered edge");
            require(orb_gpu_surface_read(gpu.get(),target,mipPixels.data(),mipPixels.size(),16),"border edge readback");
            for(size_t i=0;i<mipPixels.size();i+=4)for(unsigned c=0;c<3;++c)
                require(mipPixels[i+c]>=126 && mipPixels[i+c]<=129,"both sides blend the same black/white neighbours at a tile edge");
            require(orb_gpu_surface_release(gpu.get(),texture),"release bordered texture");
        }
        require(orb_gpu_mesh_release(gpu.get(),mipMesh),"release mip mesh");
        require(orb_gpu_surface_release(gpu.get(),mipTexture),"release mip texture");
        // DDS BGRA channel masks and padded rows must become RGBA.
        std::vector<uint8_t> rgba(140,0);std::memcpy(rgba.data(),"DDS ",4);
        word(rgba,4,124);word(rgba,8,8);word(rgba,12,1);word(rgba,16,2);word(rgba,20,12);
        word(rgba,76,32);word(rgba,80,0x41);word(rgba,88,32);
        word(rgba,92,0xff0000);word(rgba,96,0xff00);word(rgba,100,0xff);word(rgba,104,0xff000000);
        word(rgba,128,0xff123456);word(rgba,132,0xffabcdef);
        auto rgbTexture=orb_gpu_dds_create(gpu.get(),rgba.data(),rgba.size());require(rgbTexture,"masked RGBA DDS");
        OrbDraw rgbCopy{2,rgbTexture,0,0,4,4,0,0,2,1,0,0,0,0};
        require(orb_gpu_queue_batch(gpu.get(),target,&rgbCopy,1),"masked DDS blit");
        std::array<uint8_t,64> rgbaPixels{};
        require(orb_gpu_surface_read(gpu.get(),target,rgbaPixels.data(),rgbaPixels.size(),16),"masked DDS readback");
        require(rgbaPixels[0]==0x12 && rgbaPixels[1]==0x34 && rgbaPixels[2]==0x56 && rgbaPixels[12]==0xab,"DDS channel order and row pitch");
        word(rgba,100,0xff00);require(!orb_gpu_dds_create(gpu.get(),rgba.data(),rgba.size()),"overlapping channel masks rejected");
        for(unsigned kind:{1,3,5}) {
        auto odd=dds(kind,3,5);auto texture=orb_gpu_dds_create(gpu.get(),odd.data(),odd.size());require(texture,"partial BC edge blocks");
        OrbDraw copy{3,texture,0,0,4,4,0,0,3,5,0,0,0,0};require(orb_gpu_queue_batch(gpu.get(),target,&copy,1),"odd DDS blit");
        std::array<uint8_t,64> pixels{};require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),16),"odd DDS readback");
        require(pixels[60]==255 && pixels[61]==0 && pixels[62]==0,"cropped BC edge pixel");
        require(orb_gpu_surface_release(gpu.get(),texture),"release partial-block texture");
        }
        std::cout<<"DDS BC1/2/3, tiny mips, partial blocks, and malformed input verified\n";
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
