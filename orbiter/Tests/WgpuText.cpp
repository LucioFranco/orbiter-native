#include "Orbiter.h"
#include "Platform/DynamicLibrary.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char *why) {if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        require(argc==2,"graphics module required");Orbiter host;
        const std::filesystem::path path=argv[1];
        const auto module=host.LoadModule(path.parent_path().string().c_str(),path.stem().string().c_str());
        auto gc=host.GetGraphicsClient();require(module && gc && gc->clbkInitialise(),"initialize GPU");
        using Read=bool (*)(SURFHANDLE,BYTE *,size_t,DWORD);
        auto read=reinterpret_cast<Read>(orbiter::platform::LibrarySymbol(module,"orb_wgpu_read_surface"));
        const auto surface=oapiCreateSurfaceEx(128,64,OAPISURFACE_RENDERTARGET);
        auto pad=oapiGetSketchpad(surface);require(pad && read,"text target");
        std::vector<oapi::Font *> fonts;
        auto create=[&](const char *face,bool prop,FontStyle style=FONT_NORMAL,int rotation=0) {
            auto f=oapiCreateFont(24,prop,face,style,rotation);require(f,"embedded font created");fonts.push_back(f);return f;
        };
        auto sans=create("Arial",true),mono=create("Courier New",false),mocr=create("MOCR",true);
        pad->SetFont(sans);require(pad->GetTextWidth("WWW")>pad->GetTextWidth("iii"),"proportional font metrics");
        const auto cell=pad->GetCharSize();require((cell&65535)>0 && (cell&65535)<=24 && (cell>>16)>0,"font cell metrics");
        require(pad->GetTextWidth("Apollo",6)==pad->GetTextWidth("Apollo"),"explicit and automatic lengths agree");
        pad->SetFont(mono);const auto fixedWidth=pad->GetTextWidth("WWW");
        require(fixedWidth==pad->GetTextWidth("iii"),"fixed pitch metrics");
        pad->SetFont(mocr);require(pad->GetTextWidth("WWW")==fixedWidth,"MOCR maps to portable fixed font");
        pad->SetFont(create("unknown fixed face",false));require(pad->GetTextWidth("WWW")==fixedWidth,"unknown fixed font fallback");
        pad->SetFont(sans);pad->SetTextColor(RGB(255,0,0));
        using Pixels=std::array<BYTE,128*64*4>;
        auto capture=[&]() {Pixels pixels{};require(read(surface,pixels.data(),pixels.size(),128*4),"GPU text readback");return pixels;};
        auto redCount=[](const Pixels &p) {size_t count=0;for(size_t i=0;i<p.size();i+=4)count+=p[i]==255 && p[i+1]==0 && p[i+2]==0;return count;};
        auto paint=[&](const char *text,int len) {oapiColourFill(surface,0x0000ff);require(pad->Text(4,4,text,len),"draw text");return capture();};
        const auto normal=paint("Apollo",6);require(redCount(normal)>50,"glyphs produce visible pixels");
        for(size_t i=0;i<normal.size();i+=4)require((normal[i]==255 && normal[i+1]==0 && normal[i+2]==0)||(normal[i]==0 && normal[i+1]==0 && normal[i+2]==255),"transparent text preserves underlying blue");
        const auto degree=paint("\xc2\xb0",2);require(redCount(degree)>0 && degree==paint("\xb0",1),"UTF-8 and Windows-1252 degree glyph agree");
        require(redCount(paint("\xce\x94v",3))>10,"UTF-8 Greek delta supported");
        const char bounded[]={'A','p'};require(redCount(paint(bounded,2))>10,"explicit length does not read past non-terminated buffer");
        for(FontStyle style:{FONT_BOLD,FONT_ITALIC,FontStyle(FONT_BOLD|FONT_ITALIC)}) {
            pad->SetFont(create("Sans",true,style));const auto styled=paint("Apollo",6);
            require(redCount(styled)>30 && styled!=normal,"real bold/italic face changes pixels");
        }
        pad->SetFont(create("Sans",true,FontStyle(FONT_UNDERLINE|FONT_STRIKEOUT)));
        require(redCount(paint("Apollo",6))>redCount(normal),"underline and strikeout decorate text");
        pad->SetFont(sans);pad->SetBackgroundColor(RGB(0,255,0));pad->SetBackgroundMode(oapi::Sketchpad::BK_OPAQUE);
        const auto opaque=paint("Apollo",6);size_t green=0;
        for(size_t i=0;i<opaque.size();i+=4)green+=opaque[i]==0 && opaque[i+1]==255 && opaque[i+2]==0;
        require(green>50 && redCount(opaque)==redCount(normal),"opaque background fills character cells without losing text");
        pad->SetBackgroundMode(oapi::Sketchpad::BK_TRANSPARENT);
        auto bounds=[](const Pixels &p) {std::array<int,4>b{128,64,-1,-1};for(int y=0;y<64;++y)for(int x=0;x<128;++x)if(p[(y*128+x)*4]==255){b[0]=std::min(b[0],x);b[1]=std::min(b[1],y);b[2]=std::max(b[2],x);b[3]=std::max(b[3],y);}return b;};
        pad->SetTextAlign(oapi::Sketchpad::RIGHT,oapi::Sketchpad::BOTTOM);
        oapiColourFill(surface,0x0000ff);pad->Text(100,50,"Apollo",6);const auto aligned=bounds(capture());
        require(aligned[0]>20 && aligned[2]<101 && aligned[1]>20 && aligned[3]<51,"right/bottom alignment");
        pad->SetTextAlign();pad->SetOrigin(10,7);const auto moved=bounds(paint("Apollo",6)),original=bounds(normal);
        require(moved[0]==original[0]+10 && moved[1]==original[1]+7,"text origin offset");pad->SetOrigin(0,0);
        const auto upright=paint("A",1);pad->SetFont(create("Sans",true,FONT_NORMAL,900));
        oapiColourFill(surface,0x0000ff);pad->Text(40,36,"A",1);const auto rotated=capture();
        require(redCount(rotated)==redCount(upright),"right-angle rotation preserves glyph area");
        for(int y=0;y<64;++y)for(int x=0;x<128;++x)if(upright[(y*128+x)*4]==255) {
            const int rx=40+y-4,ry=36-(x-4)-1;
            require(rx>=0 && rx<128 && ry>=0 && ry<64 && rotated[(ry*128+rx)*4]==255,"text rotates around anchor");
        }
        pad->SetFont(sans);oapiColourFill(surface,0x0000ff);
        require(pad->Text(-8,-8,"Apollo",6),"negative text origin clips");require(redCount(capture())>0,"clipped text remains visible");
        oapiColourFill(surface,0x0000ff);
        const char wrapped[]={'A','p',' ','A','p'};
        const int wrapWidth=int(pad->GetTextWidth("Ap",2));
        require(pad->TextBox(4,4,4+wrapWidth,64,wrapped,5),"bounded word wrapping");const auto wrappedPixels=capture();
        oapiColourFill(surface,0x0000ff);pad->Text(4,4,"Ap",2);pad->Text(4,4+int(cell&65535),"Ap",2);
        require(wrappedPixels==capture(),"word wrapping agrees with explicit lines");
        require(!pad->TextBox(4,4,4,64,wrapped,5),"zero-width text box rejected");
        const char newlines[]={'A','\r','\n','A'};
        oapiColourFill(surface,0x0000ff);require(pad->TextBox(4,4,124,64,newlines,4),"explicit CRLF line break");
        const auto lines=capture();oapiColourFill(surface,0x0000ff);pad->Text(4,4,"A",1);pad->Text(4,4+int(cell&65535),"A",1);
        require(lines==capture(),"CRLF counts as one line break");
        oapiReleaseSketchpad(pad);for(auto font:fonts)oapiReleaseFont(font);oapiDestroySurface(surface);
        std::cout<<"Portable font maps, metrics, styles, UTF-8, alignment, backgrounds, clipping and rotation verified\n";
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
