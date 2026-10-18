#pragma once
#include "GraphicsAPI.h"
#include "NativeFont.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace orbiter::graphics {
struct NativePen : oapi::Pen {
    int style,width;DWORD color;
    NativePen(int s,int w,DWORD c):Pen(s,w,c),style(s),width(std::clamp(w,1,64)),color(c){}
};
struct NativeBrush : oapi::Brush {
    DWORD color;
    explicit NativeBrush(DWORD c):Brush(c),color(c){}
};
class NativeSketchpad : public oapi::Sketchpad {
    oapi::GraphicsClient &client;SURFHANDLE surface;
    NativePen *pen=nullptr;
    NativeBrush *brush=nullptr;
    NativeFont *font=nullptr;
    DWORD textColor=0xffffff,backgroundColor=0;
    BkgMode backgroundMode=BK_TRANSPARENT;
    TAlign_horizontal horizontal=LEFT;
    TAlign_vertical vertical=TOP;
    NativeFont &TextFont() {static NativeFont fallback(16,true,"Sans",FONT_NORMAL,0);return font?*font:fallback;}
    int ox=0,oy=0,cx=0,cy=0;
    DWORD width,height;
    static DWORD Color(DWORD bgr) {return ((bgr&255)<<16)|(bgr&0xff00)|((bgr>>16)&255);}
    // Fill pixels whose centers lie in a half-open interval, clipping before
    // integer conversion. Commands remain queued on the C++ surface.
    void Span(double left,double right,int y,DWORD color) {
        left=std::clamp(std::ceil(left-0.5),0.0,double(width));
        right=std::clamp(std::ceil(right-0.5),0.0,double(width));
        if(right>left)client.clbkFillSurface(surface,DWORD(left),DWORD(y),DWORD(right-left),1,Color(color));
    }
public:
    NativeSketchpad(oapi::GraphicsClient &c,SURFHANDLE s):Sketchpad(s),client(c),surface(s) {client.clbkGetSurfaceSize(s,&width,&height);}
    oapi::Pen *SetPen(oapi::Pen *p) override {auto old=pen;pen=static_cast<NativePen *>(p);return old;}
    oapi::Brush *SetBrush(oapi::Brush *b) override {auto old=brush;brush=static_cast<NativeBrush *>(b);return old;}
    oapi::Font *SetFont(oapi::Font *f) override {auto old=font;font=static_cast<NativeFont *>(f);return old;}
    DWORD SetTextColor(DWORD color) override {auto old=textColor;textColor=color;return old;}
    DWORD SetBackgroundColor(DWORD color) override {auto old=backgroundColor;backgroundColor=color;return old;}
    void SetBackgroundMode(BkgMode mode) override {backgroundMode=mode;}
    void SetTextAlign(TAlign_horizontal h=LEFT,TAlign_vertical v=TOP) override {horizontal=h;vertical=v;}
    DWORD GetCharSize() override {auto &f=TextFont();return DWORD(f.height)|(DWORD(f.averageWidth)<<16);}
    DWORD GetTextWidth(const char *text,int length=0) override {
        return DWORD(std::clamp(std::round(TextFont().Width(NativeFont::Decode(text,length))),0.0,double(UINT32_MAX)));
    }
    bool Text(int x,int y,const char *text,int length) override {
        if(length<0 || (!text && length))return false;
        if(!length)return true;
        return DrawText(x,y,NativeFont::Decode(text,length));
    }
    bool TextBox(int x0,int y0,int x1,int,const char *text,int length) override {
        if(length<0 || (!text && length) || x1<=x0)return false;
        if(!length)return true;
        const auto characters=NativeFont::Decode(text,length);auto &f=TextFont();
        const double available=double(x1)-x0;int64_t row=y0;
        for(size_t start=0;start<characters.size();) {
            size_t end=start,lastBreak=start;double used=0;
            for(;end<characters.size();++end) {
                const unsigned cp=characters[end];if(cp=='\n' || cp=='\r')break;
                double next=used+f.GetGlyph(cp).advance;
                if(end>start)next+=f.Kerning(characters[end-1],cp);
                if(next>available && end>start) {if(lastBreak>start)end=lastBreak;break;}
                used=next;if(cp==' ' || cp=='-')lastBreak=end+1;
            }
            size_t trimmed=end;while(trimmed>start && characters[trimmed-1]==' ')--trimmed;
            if(row>INT32_MAX)break;
            if(trimmed>start && !DrawText(x0,int(row),std::vector<unsigned>(characters.begin()+start,characters.begin()+trimmed)))return false;
            start=end;
            if(start<characters.size() && characters[start]=='\r') {++start;if(start<characters.size() && characters[start]=='\n')++start;}
            else if(start<characters.size() && characters[start]=='\n')++start;
            else while(start<characters.size() && characters[start]==' ')++start;
            row+=f.height;
        }
        return true;
    }
    bool DrawText(int x,int y,const std::vector<unsigned> &decoded) {
        auto &f=TextFont();const double advance=f.Width(decoded);
        const double alignX=horizontal==CENTER?-advance/2:horizontal==RIGHT?-advance:0;
        const double baseline=vertical==BASELINE?0:vertical==BOTTOM?f.ascent-f.height:f.ascent;
        const double angle=f.orientation*3.141592653589793/1800;
        const double cosine=std::cos(angle),sine=std::sin(angle);
        const double originX=double(x)+ox,originY=double(y)+oy;
        const auto block=[&](double left,double top,double w,double h,DWORD color) {
            if(w<=0 || h<=0)return;
            if(f.orientation==0) {
                const double a=std::clamp(std::round(originX+left),0.0,double(width));
                const double b=std::clamp(std::round(originY+top),0.0,double(height));
                const double c=std::clamp(std::round(originX+left+w),0.0,double(width));
                const double d=std::clamp(std::round(originY+top+h),0.0,double(height));
                if(c>a && d>b)client.clbkFillSurface(surface,DWORD(a),DWORD(b),DWORD(c-a),DWORD(d-b),Color(color));
            } else {
                const double px[]={left,left+w,left+w,left},py[]={top,top,top+h,top+h};
                oapi::IVECTOR2 points[4];
                for(int i=0;i<4;++i) {
                    points[i].x=int(std::clamp(std::round(originX+px[i]*cosine+py[i]*sine),double(INT32_MIN),double(INT32_MAX)));
                    points[i].y=int(std::clamp(std::round(originY-px[i]*sine+py[i]*cosine),double(INT32_MIN),double(INT32_MAX)));
                }
                NativeBrush ink(color);auto oldBrush=brush;auto oldPen=pen;
                const int oldX=ox,oldY=oy;brush=&ink;pen=nullptr;ox=oy=0;
                Polygon(points,4);brush=oldBrush;pen=oldPen;ox=oldX;oy=oldY;
            }
        };
        if(backgroundMode==BK_OPAQUE)block(alignX,baseline-f.ascent,advance,f.height,backgroundColor);
        double cursor=alignX;
        for(size_t i=0;i<decoded.size();++i) {
            const auto &glyph=f.GetGlyph(decoded[i]);
            for(const auto &run:glyph.runs)block(cursor+run.x,baseline+run.y,run.width,1,textColor);
            cursor+=glyph.advance;if(i+1<decoded.size())cursor+=f.Kerning(decoded[i],decoded[i+1]);
        }
        const int thickness=std::max(1,f.height/16);
        if(f.style&FONT_UNDERLINE)block(alignX,baseline+std::max(1,f.height/12),advance,thickness,textColor);
        if(f.style&FONT_STRIKEOUT)block(alignX,baseline-f.ascent/3,advance,thickness,textColor);
        return true;
    }
    void SetOrigin(int x,int y) override {ox=x;oy=y;}
    void GetOrigin(int *x,int *y) const override {*x=ox;*y=oy;}
    void MoveTo(int x,int y) override {cx=x;cy=y;}
    void LineTo(int x,int y) override {Line(cx,cy,x,y);cx=x;cy=y;}
    void Pixel(int x,int y,DWORD color) override {
        const auto px=int64_t(x)+ox,py=int64_t(y)+oy;
        if(px>=0 && py>=0 && px<width && py<height)client.clbkFillSurface(surface,DWORD(px),DWORD(py),1,1,Color(color));
    }
    void Line(int x0,int y0,int x1,int y1) override {
        if(!pen || !pen->style)return;
        double x=double(x0)+ox,y=double(y0)+oy,dx=double(x1)-x0,dy=double(y1)-y0,lo=0,hi=1;
        const auto clip=[&](double p,double q) {if(p==0)return q>=0;const double t=q/p;if(p<0)lo=std::max(lo,t);else hi=std::min(hi,t);return lo<=hi;};
        const double pad=pen->width;
        if(!clip(-dx,x+pad)||!clip(dx,width+pad-x)||!clip(-dy,y+pad)||!clip(dy,height+pad-y))return;
        int a=int(std::lround(x+lo*dx)),b=int(std::lround(y+lo*dy));
        const int endx=int(std::lround(x+hi*dx)),endy=int(std::lround(y+hi*dy));
        const int sx=a<endx?1:-1,sy=b<endy?1:-1;
        const int w=std::abs(endx-a),h=-std::abs(endy-b);int error=w+h,step=0;
        for(;;) {
            if(pen->style!=2 || (step/8)%2==0)client.clbkFillSurface(surface,a-pen->width/2,b-pen->width/2,pen->width,pen->width,Color(pen->color));
            if(a==endx && b==endy)break;
            const int twice=error*2;if(twice>=h){error+=h;a+=sx;}if(twice<=w){error+=w;b+=sy;}++step;
        }
    }
    void Rectangle(int x0,int y0,int x1,int y1) override {
        if(x1<x0)std::swap(x0,x1);if(y1<y0)std::swap(y0,y1);
        if(x0==x1 || y0==y1)return;
        if(brush) {
            const double left=std::clamp(double(x0)+ox,0.0,double(width));
            const double top=std::clamp(double(y0)+oy,0.0,double(height));
            const double right=std::clamp(double(x1)+ox,0.0,double(width));
            const double bottom=std::clamp(double(y1)+oy,0.0,double(height));
            if(right>left && bottom>top)client.clbkFillSurface(surface,DWORD(left),DWORD(top),DWORD(right-left),DWORD(bottom-top),Color(brush->color));
        }
        Line(x0,y0,x1-1,y0);Line(x1-1,y0,x1-1,y1-1);
        Line(x1-1,y1-1,x0,y1-1);Line(x0,y1-1,x0,y0);
    }
    void Polygon(const oapi::IVECTOR2 *points,int count) override {
        if(!points || count<2)return;
        if(brush && count>=3) {
            double top=height,bottom=0;
            for(int i=0;i<count;++i) {top=std::min(top,double(points[i].y)+oy);bottom=std::max(bottom,double(points[i].y)+oy);}
            const int first=int(std::clamp(std::ceil(top-0.5),0.0,double(height)));
            const int last=int(std::clamp(std::ceil(bottom-0.5),0.0,double(height)));
            std::vector<double> crossings;crossings.reserve(count);
            for(int y=first;y<last;++y) {
                crossings.clear();const double scan=y+0.5;
                for(int i=0,j=count-1;i<count;j=i++) {
                    const double ax=double(points[j].x)+ox,ay=double(points[j].y)+oy;
                    const double bx=double(points[i].x)+ox,by=double(points[i].y)+oy;
                    if((ay<=scan && by>scan)||(by<=scan && ay>scan))
                        crossings.push_back(ax+(scan-ay)*(bx-ax)/(by-ay));
                }
                std::sort(crossings.begin(),crossings.end());
                for(size_t i=1;i<crossings.size();i+=2)Span(crossings[i-1],crossings[i],y,brush->color);
            }
        }
        Polyline(points,count);Line(points[count-1].x,points[count-1].y,points[0].x,points[0].y);
    }
    void Ellipse(int x0,int y0,int x1,int y1) override {
        if(x1<x0)std::swap(x0,x1);if(y1<y0)std::swap(y0,y1);
        const double rx=(double(x1)-x0)/2,ry=(double(y1)-y0)/2;
        if(rx<=0 || ry<=0)return;
        const double x=double(x0)+ox+rx,y=double(y0)+oy+ry;
        const bool outline=pen && pen->style;
        const double pad=outline?pen->width/2.0:0;
        const int first=int(std::clamp(std::ceil(y-ry-pad-0.5),0.0,double(height)));
        const int last=int(std::clamp(std::ceil(y+ry+pad-0.5),0.0,double(height)));
        const auto extent=[](double a,double b,double dy) {return a>0 && b>0 && std::abs(dy)<b?a*std::sqrt(std::max(0.0,1-(dy/b)*(dy/b))):0.0;};
        for(int row=first;row<last;++row) {
            const double dy=row+0.5-y;
            if(brush) {const double half=extent(rx,ry,dy);Span(x-half,x+half,row,brush->color);}
            if(outline) {
                const double outer=extent(rx+pad,ry+pad,dy),inner=extent(rx-pad,ry-pad,dy);
                if(pen->style==2) {
                    // Dash by angular distance around the ellipse; clip the
                    // candidate pixels so giant offscreen shapes remain bounded.
                    const int left=int(std::clamp(std::ceil(x-outer-0.5),0.0,double(width)));
                    const int right=int(std::clamp(std::ceil(x+outer-0.5),0.0,double(width)));
                    for(int col=left;col<right;++col) {
                        const double dx=col+0.5-x;
                        if(std::abs(dx)<inner)continue;
                        const double angle=std::atan2(dy/ry,dx/rx)+3.141592653589793;
                        if(std::fmod(angle*std::max(rx,ry),16.0)<8)Span(col,col+1,row,pen->color);
                    }
                } else {Span(x-outer,x-inner,row,pen->color);Span(x+inner,x+outer,row,pen->color);}
            }
        }
    }
    void Polyline(const oapi::IVECTOR2 *points,int count) override {if(!points)return;for(int i=1;i<count;++i)Line(points[i-1].x,points[i-1].y,points[i].x,points[i].y);}
};
}
