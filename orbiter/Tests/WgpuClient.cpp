#include "Orbiter.h"
#include "Mesh.h"
#include <SDL3/SDL.h>
#include <vector>
#include <memory>
#include <cstring>
#include "Platform/DynamicLibrary.h"
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char **argv) {
    try {
        require(argc==4,"module and NASSP bitmap arguments required");
        Orbiter host;
        require(oapiGetColour(255,0,255)==0xff00ff && oapiGetColour(0,255,0)==0x00ff00,"native device colours are RGB32 before a window exists");
        const std::filesystem::path modulePath=argv[1];
        auto module=host.LoadModule(modulePath.parent_path().string().c_str(),modulePath.stem().string().c_str());
        require(module && host.GetGraphicsClient(),"load and register graphics plugin");
        auto gc=host.GetGraphicsClient();
        require(gc->clbkInitialise(),"initialize GPU");
        using Read = bool (*)(SURFHANDLE, BYTE *, size_t, DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));
        require(read != nullptr,"readback export");
        auto surface=oapiCreateSurfaceEx(3,2,OAPISURFACE_RENDERTARGET);
        require(surface != nullptr,"SDK creates GPU surface");
        std::array<BYTE,24> pixels{};
        for(unsigned i=0;i<6;++i) { pixels[i*4]=1; pixels[i*4+3]=255; }
        require(oapiUpdateSurface(surface,pixels.data(),12),"SDK uploads pixels");
        oapiColourFill(surface,gc->clbkGetDeviceColour(255,0,0),0,0,2,2);
        oapiColourFill(surface,gc->clbkGetDeviceColour(0,255,0),1,1,2,1);
        host.RenderNativeFrame(); // flush one queued batch for this surface
        require(read(surface,pixels.data(),pixels.size(),12),"read filled surface");
        const std::array<BYTE,24> expected{255,0,0,255,255,0,0,255,1,0,0,255,
                                        255,0,0,255,0,255,0,255,0,255,0,255};
        require(pixels==expected,"SDK fills preserve ordering and device RGB format");
        gc->clbkIncrSurfaceRef(surface);
        require(gc->clbkReleaseSurface(surface),"release shared reference");
        require(read(surface,pixels.data(),pixels.size(),12),"remaining reference stays alive");
        require(gc->clbkReleaseSurface(surface),"release final reference");
        require(!read(surface,pixels.data(),pixels.size(),12),"released surface unavailable");
        auto source=oapiCreateSurfaceEx(2,1,OAPISURFACE_TEXTURE);
        auto target=oapiCreateSurfaceEx(4,2,OAPISURFACE_RENDERTARGET);
        const std::array<BYTE,8> sprite{255,0,0,255,255,0,255,255};
        require(source && target && oapiUpdateSurface(source,sprite.data(),8),"create blit surfaces");
        require(gc->clbkSetSurfaceColourKey(source,0xff00ff),"set magenta colour key");
        oapiColourFill(target,0x0000ff);
        require(gc->clbkScaleBlt(target,0,0,4,2,source,0,0,2,1,BLT_SRCCOLORKEY),"queue scaled colour-key blit");
        oapiColourFill(source,0x00ff00); // must not change the already-recorded source read
        require(gc->clbkBlt(target,1,1,source,0,0,1,1,0),"queue later source read");
        require(gc->clbkBlt(target,1,0,target,0,0,3,1,0),"queue overlapping self blit");
        // Exercise the SDK route used by NASSP, not just direct plugin calls.
        auto keyed=oapiCreateSurfaceEx(2,1,OAPISURFACE_TEXTURE);
        auto keyedTarget=oapiCreateSurfaceEx(4,3,OAPISURFACE_RENDERTARGET);
        require(keyed && keyedTarget && oapiUpdateSurface(keyed,sprite.data(),8),"SDK key fixture");
        oapiSetSurfaceColourKey(keyed,0xff00ff);oapiColourFill(keyedTarget,0x0000ff);
        oapiBlt(keyedTarget,keyed,0,0,0,0,2,1); // native default uses the predefined key
        oapiBlt(keyedTarget,keyed,0,1,0,0,2,1,0xff0000); // explicit red, leave predefined magenta intact
        oapiBlt(keyedTarget,keyed,2,0,0,0,2,1,SURF_NO_CK);
        RECT dstRect{0,2,4,3},srcRect{0,0,2,1};
        oapiBlt(keyedTarget,keyed,&dstRect,&srcRect,SURF_PREDEF_CK);
        std::array<BYTE,48> keys{};require(read(keyedTarget,keys.data(),keys.size(),16),"SDK colour-key pixels");
        const std::array<BYTE,48> keyExpected{
            255,0,0,255, 0,0,255,255, 255,0,0,255, 255,0,255,255,
            0,0,255,255, 255,0,255,255, 0,0,255,255, 0,0,255,255,
            255,0,0,255, 255,0,0,255, 0,0,255,255, 0,0,255,255};
        require(keys==keyExpected,"SDK predefined/explicit/no-key and scaled blits preserve key state");
        oapiDestroySurface(keyed);oapiDestroySurface(keyedTarget);
        require(gc->clbkReleaseSurface(source),"source release submits its queued consumers");
        std::array<BYTE,32> blitted{};
        require(read(target,blitted.data(),blitted.size(),16),"read blit result");
        const std::array<BYTE,32> blitExpected{
            255,0,0,255,255,0,0,255,255,0,0,255,0,0,255,255,
            255,0,0,255,0,255,0,255,0,0,255,255,0,0,255,255};
        require(blitted==blitExpected,"scaled keying, cross-surface ordering and self-copy snapshot");
        oapiColourFill(target,0xffff00,-1,-1,2,2);
        require(read(target,blitted.data(),blitted.size(),16),"read clipped fill");
        auto clipped=blitExpected;
        clipped[0]=255; clipped[1]=255; clipped[2]=0;
        require(blitted==clipped,"negative destination coordinates clip correctly");
        require(gc->clbkReleaseSurface(target),"release blit target");
        auto ink=oapiCreateSurfaceEx(8,8,OAPISURFACE_RENDERTARGET);
        oapiColourFill(ink,0);
        auto pen=gc->clbkCreatePen(1,1,RGB(255,0,0));
        auto pad=gc->clbkGetSketchpad(ink);require(pad && pen,"native Sketchpad and pen");
        pad->SetBackgroundMode(oapi::Sketchpad::BK_TRANSPARENT);pad->SetPen(pen);
        const oapi::IVECTOR2 points[]={{-100,2},{3,2},{3,6}};pad->Polyline(points,3);
        gc->clbkReleaseSketchpad(pad);gc->clbkReleasePen(pen);
        std::array<BYTE,256> linePixels{};require(read(ink,linePixels.data(),linePixels.size(),32),"Sketchpad pixels");
        for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
            const auto pixel=&linePixels[(y*8+x)*4];const bool red=(y==2 && x<=3)||(x==3 && y>=2 && y<=6);
            require(pixel[0]==(red?255:0) && pixel[1]==0 && pixel[2]==0,"clipped polyline and BGR pen conversion");
        }
        gc->clbkReleaseSurface(ink);
        auto shapes=oapiCreateSurfaceEx(16,16,OAPISURFACE_RENDERTARGET);
        auto green=gc->clbkCreateBrush(RGB(0,255,0));
        auto red=gc->clbkCreatePen(1,1,RGB(255,0,0));
        pad=gc->clbkGetSketchpad(shapes);require(pad && green && red,"native brush resources");
        require(pad->SetBrush(green)==nullptr,"select first brush");pad->SetPen(red);
        std::array<BYTE,16*16*4> shapePixels{};
        auto readShapes=[&]() {require(read(shapes,shapePixels.data(),shapePixels.size(),64),"read batched shapes");};
        auto checkPixel=[&](int x,int y,int r,int g,int b) {
            const auto p=&shapePixels[(y*16+x)*4];require(p[0]==r && p[1]==g && p[2]==b,"shape pixel coverage and colour");
        };
        oapiColourFill(shapes,0);pad->SetOrigin(-2,1);pad->Rectangle(0,0,6,5);readShapes();
        for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
            const bool inside=x<4 && y>=1 && y<6;
            const bool edge=inside && (x==3 || y==1 || y==5);
            checkPixel(x,y,edge?255:0,inside && !edge?255:0,0);
        }
        pad->SetOrigin(0,0);require(pad->SetPen(nullptr)==red,"disable outline");
        oapi::IVECTOR2 concave[]={{1,1},{7,1},{7,3},{3,3},{3,7},{1,7}};
        for(int winding=0;winding<2;++winding) {
            oapiColourFill(shapes,0);pad->Polygon(concave,6);readShapes();
            for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
                const bool inside=x>=1 && y>=1 && ((x<7 && y<3)||(x<3 && y<7));
                checkPixel(x,y,0,inside?255:0,0);
            }
            std::reverse(std::begin(concave),std::end(concave));
        }
        const oapi::IVECTOR2 crossed[]={{2,2},{12,12},{2,12},{12,2}};
        oapiColourFill(shapes,0);pad->Polygon(crossed,4);readShapes();
        checkPixel(7,3,0,255,0);checkPixel(7,10,0,255,0);checkPixel(3,7,0,0,0);
        pad->SetBrush(nullptr);pad->SetPen(red);oapiColourFill(shapes,0);
        const oapi::IVECTOR2 triangle[]={{2,2},{12,2},{7,12}};
        pad->Polygon(triangle,3);readShapes();
        checkPixel(7,2,255,0,0);checkPixel(4,6,255,0,0);checkPixel(7,6,0,0,0);
        pad->SetBrush(green);
        pad->SetPen(red);oapiColourFill(shapes,0);pad->Ellipse(2,2,14,14);readShapes();
        for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
            const double d2=(x-7.5)*(x-7.5)+(y-7.5)*(y-7.5);
            const bool edge=d2<6.5*6.5 && d2>=5.5*5.5;
            checkPixel(x,y,edge?255:0,d2<5.5*5.5?255:0,0);
        }
        require(pad->SetBrush(nullptr)==green,"disable fill");
        oapiColourFill(shapes,0);pad->Ellipse(2,4,14,12);readShapes();
        checkPixel(8,8,0,0,0);checkPixel(2,8,255,0,0);checkPixel(8,4,255,0,0);
        // Exercise clipping with coordinates whose subtraction would overflow int.
        pad->SetPen(nullptr);pad->SetBrush(green);pad->SetOrigin(10,-10);
        const oapi::IVECTOR2 huge[]={{INT32_MIN,INT32_MIN},{INT32_MAX,INT32_MIN},{INT32_MAX,INT32_MAX},{INT32_MIN,INT32_MAX}};
        oapiColourFill(shapes,0);pad->Polygon(huge,4);readShapes();
        for(int y=0;y<16;++y)for(int x=0;x<16;++x)checkPixel(x,y,0,255,0);
        gc->clbkReleaseSketchpad(pad);gc->clbkReleaseBrush(green);gc->clbkReleasePen(red);gc->clbkReleaseSurface(shapes);
        auto bitmap=oapiLoadSurfaceEx(argv[2],OAPISURFACE_TEXTURE,true);
        require(bitmap != nullptr,"load real NASSP resource BMP via stb_image");
        DWORD width=0,height=0;
        require(gc->clbkGetSurfaceSize(bitmap,&width,&height) && width>0 && height>0,"NASSP bitmap dimensions");
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> original(SDL_LoadBMP(argv[3]),SDL_DestroySurface);
        require(bool(original),"independent SDL decode of original RLE bitmap");
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> reference(
            SDL_ConvertSurface(original.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
        require(bool(reference) && unsigned(reference->w)==width && unsigned(reference->h)==height,"reference bitmap dimensions");
        std::vector<BYTE> decoded(size_t(width)*height*4);
        require(read(bitmap,decoded.data(),decoded.size(),width*4),"read normalized resource from GPU");
        for (unsigned y=0;y<height;++y)
            require(!std::memcmp(decoded.data()+y*width*4,static_cast<BYTE *>(reference->pixels)+y*reference->pitch,width*4),
                "normalized GPU bitmap matches original RLE decode");
        gc->clbkIncrSurfaceRef(bitmap); // transfer one reference to a template mesh
        {
            Mesh source;source.AddTexture(bitmap);
            {Mesh visual(source);require(visual.GetTexture(0)==bitmap,"copied mesh texture reference");}
            require(read(bitmap,decoded.data(),decoded.size(),width*4),"copy destruction preserves template texture");
        }
        require(read(bitmap,decoded.data(),decoded.size(),width*4),"template destruction preserves caller texture reference");
        require(gc->clbkReleaseSurface(bitmap),"release bitmap");
        {
            using Preview=bool (*)(MESHHANDLE,const float *,DWORD,DWORD);
            using ReadOutput=bool (*)(BYTE *,size_t,DWORD);
            auto preview=reinterpret_cast<Preview>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_preview_mesh"));
            auto readOutput=reinterpret_cast<ReadOutput>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_output"));
            require(preview && readOutput,"material rendering diagnostics");
            Mesh materialMesh;
            D3DMATERIAL7 glass{},backdrop{};
            glass.diffuse={1,0,0,.05f};glass.emissive={1,1,1,1};
            backdrop.diffuse={0,0,1,1};backdrop.emissive={1,1,1,1};
            materialMesh.AddMaterial(glass);materialMesh.AddMaterial(backdrop);
            const WORD indices[]={0,1,2};
            const float xy[3][2]={{-1,-1},{3,-1},{-1,3}};
            for(unsigned layer=0;layer<2;++layer) {
                NTVERTEX vertices[3]{};const float z=layer?4.f:2.f;
                for(unsigned i=0;i<3;++i) {
                    vertices[i].x=xy[i][0]*z;vertices[i].y=xy[i][1]*z;vertices[i].z=z;vertices[i].nz=-1;
                }
                WORD ownedIndices[]={indices[0],indices[1],indices[2]};
                materialMesh.AddGroup(vertices,3,ownedIndices,3,layer,SPEC_DEFAULT,0,0,true);
            }
            float projection[16]{};projection[0]=projection[5]=projection[11]=1;projection[14]=.01f;
            std::array<BYTE,8*8*4> output{};
            for(float alpha:{0.f,.05f,.11f,.5f,.75f,1.f}) {
                materialMesh.GetMaterial(0)->diffuse.a=alpha;
                require(preview(&materialMesh,projection,8,8),"upload original material opacity");
                host.RenderNativeFrame();
                require(readOutput(output.data(),output.size(),32),"read blended materials");
                const auto pixel=output.data()+(4*8+4)*4;
                require(std::abs(int(pixel[0])-int(alpha*255))<=1 && pixel[1]==0 &&
                        std::abs(int(pixel[2])-int((1-alpha)*255))<=1,
                        "material opacity blends continuously, including faint glass and opaque endpoints");
            }
        }
        require(host.UnloadModule(module) && !host.GetGraphicsClient(),"unload unregisters client");
        std::cout << "SDK GPU surfaces, batching, references and NASSP bitmap load verified\n";
        return 0;
    } catch(const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
