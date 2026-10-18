#pragma once
#include "DrawAPI.h"
#include <stb_truetype.h>
#include <unordered_map>
#include <vector>
namespace orbiter::graphics {
class NativeFont : public oapi::Font {
public:
    struct Run {int x,y,width;};
    struct Glyph {float advance=0;std::vector<Run> runs;};
    NativeFont(int height,bool prop,const char *face,FontStyle style,int orientation);
    const Glyph &GetGlyph(unsigned codepoint);
    float Kerning(unsigned left,unsigned right) const;
    double Width(const std::vector<unsigned> &text);
    static std::vector<unsigned> Decode(const char *text,int length);
    int height=0,ascent=0,averageWidth=0,orientation=0;
    FontStyle style;
private:
    stbtt_fontinfo info{};
    float scale=0;
    std::unordered_map<unsigned,Glyph> glyphs;
};
}
