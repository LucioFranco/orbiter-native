#define STB_TRUETYPE_IMPLEMENTATION
#include "NativeFont.h"
#include "NativeFontData.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
namespace orbiter::graphics {
NativeFont::NativeFont(int requested,bool prop,const char *face,FontStyle decoration,int angle)
    :Font(requested,prop,face,decoration,angle),orientation(angle%3600),style(decoration) {
    std::string name=face?face:"";
    for(char &c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    bool fixed=!prop;
    if(name=="courier" || name=="courier new" || name=="fixed" || name=="mocr")fixed=true;
    if(name=="arial" || name=="helvetica" || name=="sans" || name=="serif")fixed=false;
    const unsigned char *sans[]={fonts::Roboto_Regular,fonts::Roboto_Bold,fonts::Roboto_Italic,fonts::Roboto_BoldItalic};
    const unsigned char *mono[]={fonts::JetBrainsMono_Regular,fonts::JetBrainsMono_Bold,fonts::JetBrainsMono_Italic,fonts::JetBrainsMono_BoldItalic};
    const auto data=(fixed?mono:sans)[int(style)&3];
    if(!stbtt_InitFont(&info,data,stbtt_GetFontOffsetForIndex(data,0)))throw std::runtime_error("Invalid embedded font");
    const int pixels=int(std::clamp(std::abs(double(requested)),1.0,512.0));
    int asc,desc,gap;stbtt_GetFontVMetrics(&info,&asc,&desc,&gap);
    scale=float(pixels)/float(asc-desc+(requested>0?gap:0));
    height=std::max(1,int(std::ceil((asc-desc)*scale)));
    ascent=int(std::ceil(asc*scale));
    int advance,bearing;stbtt_GetCodepointHMetrics(&info,'M',&advance,&bearing);
    averageWidth=std::max(1,int(std::lround(advance*scale)));
}
const NativeFont::Glyph &NativeFont::GetGlyph(unsigned codepoint) {
    const auto found=glyphs.find(codepoint);if(found!=glyphs.end())return found->second;
    Glyph glyph;int advance,bearing,w,h,xoff,yoff;
    stbtt_GetCodepointHMetrics(&info,int(codepoint),&advance,&bearing);glyph.advance=advance*scale;
    auto bitmap=stbtt_GetCodepointBitmap(&info,scale,scale,int(codepoint),&w,&h,&xoff,&yoff);
    if(bitmap) {
        // Crisp coverage is recorded as horizontal runs rather than per-pixel
        // foreign calls. Glyphs are cached for the lifetime of the font.
        for(int y=0;y<h;++y)for(int x=0;x<w;) {
            if(bitmap[y*w+x]<128) {++x;continue;}
            const int start=x;while(x<w && bitmap[y*w+x]>=128)++x;
            glyph.runs.push_back({start+xoff,y+yoff,x-start});
        }
        stbtt_FreeBitmap(bitmap,nullptr);
    }
    return glyphs.emplace(codepoint,std::move(glyph)).first->second;
}
float NativeFont::Kerning(unsigned left,unsigned right) const {return stbtt_GetCodepointKernAdvance(&info,int(left),int(right))*scale;}
double NativeFont::Width(const std::vector<unsigned> &text) {
    double width=0;
    for(size_t i=0;i<text.size();++i) {width+=GetGlyph(text[i]).advance;if(i+1<text.size())width+=Kerning(text[i],text[i+1]);}
    return width;
}
std::vector<unsigned> NativeFont::Decode(const char *text,int length) {
    std::vector<unsigned> out;if(!text || length<0)return out;
    const size_t size=length?size_t(length):std::strlen(text);
    const auto bytes=reinterpret_cast<const unsigned char *>(text);
    constexpr unsigned cp1252[]={0x20ac,0xfffd,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0xfffd,0x17d,0xfffd,0xfffd,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0xfffd,0x17e,0x178};
    for(size_t i=0;i<size && bytes[i];) {
        unsigned cp=bytes[i];size_t count=1;
        const int extra=cp>=0xc2 && cp<=0xdf?1:cp>=0xe0 && cp<=0xef?2:cp>=0xf0 && cp<=0xf4?3:0;
        if(extra && i+extra<size) {
            unsigned decoded=cp&((1u<<(6-extra))-1);bool valid=true;
            for(int j=1;j<=extra;++j) {valid&=(bytes[i+j]&0xc0)==0x80;decoded=(decoded<<6)|(bytes[i+j]&63);}
            valid&=decoded>=(extra==1?0x80u:extra==2?0x800u:0x10000u) && decoded<=0x10ffff && !(decoded>=0xd800 && decoded<=0xdfff);
            if(valid) {cp=decoded;count=extra+1;}
        }
        if(count==1 && cp>=0x80 && cp<0xa0)cp=cp1252[cp-0x80];
        out.push_back(cp);i+=count;
    }
    return out;
}
}
