#include "boot_splash.h"
#include <string.h>

namespace {
    const u32 BG      = 0x081525;
    const u32 DEEP    = 0x10263B;
    const u32 PANEL   = 0x101D2F;
    const u32 EDGE    = 0x34516A;
    const u32 MINT    = 0x5CE0C9;
    const u32 WHITE   = 0xE9F5FB;
    const u32 SECOND  = 0x9CB7C8;
    const u32 AMBER   = 0xF3BD64;
    const u32 RED     = 0xF28D89;
}

CBootSplash::CBootSplash()
: base_(0),width_(0),height_(0),pitch_(0),depth_(0),count_(0),step_(0),
  initialized_(FALSE),done_(FALSE),font_(Font8x16)
{
    final_[0] = '\0';
    memset(lines_,0,sizeof(lines_));
    memset(statuses_,0,sizeof(statuses_));
}

boolean CBootSplash::Begin(CBcmFrameBuffer *fb)
{
    if (!fb || !fb->GetBuffer()) return FALSE;
    width_=fb->GetWidth(); height_=fb->GetHeight();
    pitch_=fb->GetPitch(); depth_=fb->GetDepth();
    if ((depth_!=16 && depth_!=32) || width_<640 || height_<480 ||
        width_>4096 || height_>2160 || pitch_<width_*(depth_/8)) return FALSE;
    base_=(u8*)(unsigned long)fb->GetBuffer();
    count_=step_=0; done_=FALSE; initialized_=TRUE;
    FullBackground();
    RefreshProgress();
    RefreshConsole();
    return TRUE;
}

void CBootSplash::Add(char *dst,unsigned cap,const char *src)
{
    if (!src || !cap) return;
    unsigned n=(unsigned)strlen(dst);
    while (*src && n+1<cap) dst[n++]=*src++;
    dst[n]=0;
}

u32 CBootSplash::Color(u32 rgb) const
{
    if (depth_==16) {
        return ((rgb>>8)&0xF800) | ((rgb>>5)&0x07E0) | ((rgb>>3)&0x001F);
    }
    return 0xFF000000u | (rgb&0xFFFFFFu);
}

void CBootSplash::Pixel(unsigned x,unsigned y,u32 rgb)
{
    if (!initialized_ || x>=width_ || y>=height_) return;
    u8 *dst=base_+y*pitch_+x*(depth_/8);
    const u32 converted=Color(rgb);
    if (depth_==16) {
        const u16 v=(u16)converted;
        memcpy(dst,&v,2);
    } else {
        memcpy(dst,&converted,4);
    }
}

void CBootSplash::Rect(int x,int y,int w,int h,u32 rgb)
{
    if (!initialized_ || w<=0 || h<=0) return;
    if (x<0) {w+=x; x=0;} if(y<0) {h+=y; y=0;}
    if (x+w>(int)width_) w=(int)width_-x;
    if (y+h>(int)height_) h=(int)height_-y;
    if (w<=0 || h<=0) return;
    const u32 color=Color(rgb);
    const unsigned bpp=depth_/8;
    for(int yy=y; yy<y+h; ++yy) {
        u8 *dst=base_+(unsigned)yy*pitch_+(unsigned)x*bpp;
        if (bpp==4) {
            for(int xx=0;xx<w;xx++) memcpy(dst+xx*4,&color,4);
        } else {
            const u16 c=(u16)color;
            for(int xx=0;xx<w;xx++) memcpy(dst+xx*2,&c,2);
        }
    }
}

void CBootSplash::Text(int x,int y,const char *msg,u32 rgb,unsigned scale)
{
    if (!msg || !scale) return;
    const unsigned fw=font_.GetCharWidth();
    const unsigned fh=font_.GetCharHeight();
    const int x0=x;
    while (*msg) {
        char ch=*msg++;
        if (ch=='\n') {y+=(int)(fh*scale); x=x0; continue;}
        if (ch<' ' || ch>126) ch='?';
        if (x+(int)(fw*scale)>(int)width_) break;
        for(unsigned cy=0; cy<fh; ++cy) {
            const CCharGenerator::TPixelLine bits=font_.GetPixelLine(ch,cy);
            for(unsigned cx=0;cx<fw;cx++) {
                if(font_.GetPixel(cx,bits)) {
                    for(unsigned sy=0;sy<scale;sy++)for(unsigned sx=0;sx<scale;sx++)
                        Pixel((unsigned)(x+(int)((cx*scale)+sx)),
                              (unsigned)(y+(int)((cy*scale)+sy)),rgb);
                }
            }
        }
        x+=(int)(fw*scale);
    }
}

void CBootSplash::FullBackground()
{
    // Minimal XD OS identity; later replace this with ported original bitmap/assets.
    const int W=(int)width_,H=(int)height_;
    Rect(0,0,W,H,BG);
    Rect(0,0,W,9,MINT);
    Rect(0,H-42,W,42,DEEP);
    Rect(0,H-42,W,1,EDGE);

    // Responsive layout: centered content scaled/clamped for 800x600 or 1024x768.
    const int left=(W-880)/2>22?(W-880)/2:22;
    const int contentW=W-2*left;
    Rect(left,64,82,82,0x1B687E);
    Rect(left+11,75,60,60,0x193A57);
    Rect(left+21,86,12,39,MINT);
    Rect(left+43,86,12,39,MINT);
    Rect(left+26,100,25,9,0x193A57);
    Text(left+108,74,"XD OS",WHITE,3);
    Text(left+111,131,"ARM32 BARE-METAL  /  RASPBERRY PI 3B / 3B+",SECOND,1);
    Text(left,198,"SYSTEM INITIALIZATION",MINT,1);
    Text(left,230,"Loading hardware drivers and desktop components",SECOND,1);
    Rect(left,273,contentW,1,EDGE);
    Rect(left,304,contentW,H-363,PANEL);
    Rect(left,304,contentW,2,EDGE);
    Text(left+20,319,"BOOT DIAGNOSTICS",MINT,1);
    Text(left+20,H-58,"Serial UART: 115200 baud / 8N1",SECOND,1);
    Text(left,H-29,"XD OS ARM32  |  Circle USB / SDIO / Ethernet  |  NO LINUX",SECOND,1);
}

void CBootSplash::RefreshProgress()
{
    if (!initialized_) return;
    const int W=(int)width_;
    const int left=(W-880)/2>22?(W-880)/2:22;
    const int contentW=W-2*left;
    Rect(left,256,contentW,10,EDGE);
    const unsigned completed=step_>12?12:step_;
    Rect(left,256,contentW*(int)completed/12,10,done_?MINT:0x2BBAB1);
}

void CBootSplash::RefreshConsole()
{
    if(!initialized_)return;
    const int W=(int)width_,H=(int)height_;
    const int left=(W-880)/2>22?(W-880)/2:22;
    const int contentW=W-2*left;
    const int top=350;
    const int available=H-410;
    if(available<=0)return;
    Rect(left+13,top,contentW-26,available,PANEL);
    unsigned visible=(unsigned)available/19;
    if(visible>MAX_LINES)visible=MAX_LINES;
    unsigned start=count_>visible?count_-visible:0;
    for(unsigned i=start;i<count_;++i){
        const int y=top+(int)((i-start)*19);
        u32 color=WHITE;
        if(statuses_[i]==WARNING || statuses_[i]==PENDING)color=AMBER;
        if(statuses_[i]==ERROR)color=RED;
        Text(left+20,y,lines_[i],color,1);
    }
    if(done_) {
        Rect(left+16,H-102,contentW-32,24,0x164B50);
        Text(left+25,H-98,final_,MINT,1);
    }
}

void CBootSplash::Report(unsigned step,const char *component,const char *detail,TStatus status)
{
    if(!initialized_)return;
    if(step>step_)step_=step;
    if(count_>=MAX_LINES){
        memmove(lines_,lines_+1,sizeof(lines_)-sizeof(lines_[0]));
        memmove(statuses_,statuses_+1,sizeof(statuses_)-sizeof(statuses_[0]));
        count_=MAX_LINES-1;
    }
    char *dst=lines_[count_]; dst[0]='\0';
    Add(dst,MAX_CHARS,"[");
    if(step<10)Add(dst,MAX_CHARS,"0");
    char number[3];number[0]=(char)('0'+(step/10)%10);number[1]=(char)('0'+step%10);number[2]=0;
    if(step<10)Add(dst,MAX_CHARS,number+1);
    else Add(dst,MAX_CHARS,number);
    Add(dst,MAX_CHARS,"] ");
    switch(status){
        case OK: Add(dst,MAX_CHARS,"OK   ");break;
        case PENDING:Add(dst,MAX_CHARS,"WAIT ");break;
        case WARNING:Add(dst,MAX_CHARS,"WARN ");break;
        default:Add(dst,MAX_CHARS,"FAIL ");break;
    }
    Add(dst,MAX_CHARS,component?component:"");
    Add(dst,MAX_CHARS," - ");
    Add(dst,MAX_CHARS,detail?detail:"");
    statuses_[count_]=status;
    ++count_;
    RefreshProgress();RefreshConsole();
}

void CBootSplash::Finish(const char *message)
{
    if(!initialized_)return;
    done_=TRUE;
    step_=12;
    final_[0]='\0';
    Add(final_,sizeof(final_),message ? message : "BOOT COMPLETE. Opening desktop...");
    RefreshProgress();
    RefreshConsole();
}
