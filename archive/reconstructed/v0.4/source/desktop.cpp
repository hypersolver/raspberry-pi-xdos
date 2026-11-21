#include "desktop.h"
#include "asm_gui.h"
#include <circle/chargenerator.h>
#include <circle/font.h>
#include <string.h>

namespace {
static const unsigned BG=0x122238, BG2=0x182F48, NAV=0x0A1727;
static const unsigned SURFACE=0xF8FAFD, FRAME=0xB6C4D3, INK=0x203047;
static const unsigned DIM=0x72859A, ACCENT=0x4FC7BB, WHITE=0xFFFFFF;
static const unsigned BAR=0x294A6A, RED=0xF27771, CYAN=0xBEECE6;
static const char *names[] = {"Terminal","Notepad","Calculator","Paint","Files","Browser"};
static const char *icons[] = {"$","N","+","P","F","W"};
static CCharGenerator font(Font8x16);
static void LongToString(long n, char out[32])
{
    char tmp[32]; unsigned idx=0, pos=0;
    bool neg=n<0;
    unsigned long v=neg ? (unsigned long)(-(n+1))+1u : (unsigned long)n;
    do {tmp[idx++]=(char)('0'+v%10u);v/=10u;} while(v && idx<sizeof(tmp)-1);
    if(neg)out[pos++]='-';
    while(idx)out[pos++]=tmp[--idx];
    out[pos]=0;
}
}

int Desktop::Clamp(int value,int lo,int hi) { return value<lo?lo:(value>hi?hi:value); }
bool Desktop::Contains(int x,int y,int rx,int ry,int rw,int rh)
{ return x>=rx && y>=ry && x<rx+rw && y<ry+rh; }

Desktop::Desktop() : markCount_(0),
    netRunning_(false),browserPending_(false),browserLoading_(false),
    browserRequestID_(0),
    damagePartial_(false),damageX0_(0),damageY0_(0),damageX1_(0),damageY1_(0),
    termLen_(0),noteLen_(0),cmdLen_(0),calcLen_(0),
    calcValue_(0),calcEntry_(0),calcOp_(0),calcNewEntry_(false),
    width_(1024),height_(768),active_(0),dragIndex_(-1),dragDX_(0),dragDY_(0),
    mouseX_(0),mouseY_(0),dirty_(true),keyboard_(false),mouse_(false),
    framebuffer_(0),pitchBytes_(0),depth_(0)
{
    windows_[0]={152,120,626,380,true};
    windows_[1]={440,250,468,360,true};
    windows_[2]={280,185,360,420,false};
    windows_[3]={220,155,620,440,false};
    windows_[4]={260,170,500,350,false};
    windows_[5]={300,200,520,340,false};
    terminal_[0]=note_[0]=cmd_[0]='\0';
    strcpy(browserURL_,"http://example.com/");
    strcpy(browserContent_,"Enter an HTTP URL and press Enter to load.\nHTTPS is not yet supported.\n");
    strcpy(browserStatus_,"Ready - HTTP only");
    strcpy(netStatus_,"WAITING FOR DHCP");
    strcpy(calcDisplay_,"0");
    Print("XD OS ARM32 V0.4 RECONSTRUCTED  |  Raspberry Pi 3 bare-metal\n");
    Print("Circle USB + WLAN/Ethernet + HTTP Browser (timeout)\n");
    Print("Type help then Enter to see commands.\n\n");
    Prompt();
}

void Desktop::Resize(int w,int h)
{
    width_=w; height_=h; dirty_=true;
    for (int i=0;i<NWIN;i++) {
        if (windows_[i].w>width_-20) windows_[i].w=width_-20;
        if (windows_[i].h>height_-65) windows_[i].h=height_-65;
        windows_[i].x=Clamp(windows_[i].x,0,width_-windows_[i].w);
        windows_[i].y=Clamp(windows_[i].y,0,height_-windows_[i].h-42);
    }
}

void Desktop::SetUSB(bool keyboard,bool mouse)
{ if (keyboard_!=keyboard||mouse_!=mouse) dirty_=true; keyboard_=keyboard; mouse_=mouse; }

void Desktop::SetNetwork(bool running,const char *status)
{
    if(!status)status="NETWORK ERROR";
    if(netRunning_!=running || strcmp(netStatus_,status)) {
        netRunning_=running;
        strncpy(netStatus_,status,sizeof(netStatus_)-1);
        netStatus_[sizeof(netStatus_)-1]=0;
        dirty_=true;
    }
}
void Desktop::BrowserStatus(const char *message)
{
    if(!message)message="";
    strncpy(browserStatus_,message,sizeof(browserStatus_)-1);
    browserStatus_[sizeof(browserStatus_)-1]=0;
    dirty_=true;
}
void Desktop::BrowserError(const char *message)
{
    browserLoading_=false;
    BrowserStatus(message);
    strncpy(browserContent_,message,sizeof(browserContent_)-1);
    browserContent_[sizeof(browserContent_)-1]=0;
    dirty_=true;
}
void Desktop::SubmitBrowserURL()
{
    if(browserLoading_)return;
    ++browserRequestID_;
    browserPending_=true;
    browserLoading_=true;
    strcpy(browserContent_,"Loading page...");
    BrowserStatus("Queued HTTP GET...");
}
void Desktop::CancelBrowser()
{
    if (!browserLoading_) return;
    ++browserRequestID_; // invalidate results from the previous request
    browserPending_=false;
    browserLoading_=false;
    BrowserStatus("Cancelled - press Enter to retry");
    strcpy(browserContent_, "Page loading cancelled.\n");
}
bool Desktop::BrowserRequestActive(unsigned requestID) const
{
    return browserLoading_ && browserRequestID_ == requestID;
}
bool Desktop::TakeBrowserRequest(char *out,unsigned size,unsigned &requestID)
{
    if(!browserPending_||!out||size<sizeof(browserURL_))return false;
    strcpy(out,browserURL_);
    requestID=browserRequestID_;
    browserPending_=false;
    return true;
}
void Desktop::BrowserLoaded(const char *html,unsigned length)
{
    // Minimal bounded HTML-to-text, not a full browser layout engine.
    // Convert tags to spaces or newlines. Drop <script>/<style> blocks.
    unsigned out=0;
    bool tag=false,space=false,skip=false;
    char tagbuf[32];unsigned tn=0;
    for(unsigned i=0;i<length && html[i] && out<sizeof(browserContent_)-2;++i) {
        char c=html[i];
        if(c=='<') {tag=true;tn=0;continue;}
        if(tag) {
            if(c=='>') {
                tag=false;tagbuf[tn]=0;
                // primitive script/style exclusion, case insensitive for common tags
                if(!strncmp(tagbuf,"script",6)||!strncmp(tagbuf,"SCRIPT",6)||
                   !strncmp(tagbuf,"style",5)||!strncmp(tagbuf,"STYLE",5))skip=true;
                if(!strncmp(tagbuf,"/script",7)||!strncmp(tagbuf,"/SCRIPT",7)||
                   !strncmp(tagbuf,"/style",6)||!strncmp(tagbuf,"/STYLE",6))skip=false;
                if(!skip && (tagbuf[0]=='/'||tagbuf[0]=='b'||tagbuf[0]=='B'||
                              tagbuf[0]=='h'||tagbuf[0]=='H'||tagbuf[0]=='p'||tagbuf[0]=='P'||
                              tagbuf[0]=='l'||tagbuf[0]=='L')) {
                    if(out && browserContent_[out-1]!='\n')browserContent_[out++]='\n';
                }
            } else if(tn<sizeof(tagbuf)-1)tagbuf[tn++]=c;
            continue;
        }
        if(skip)continue;
        if(c=='&') {
            if(i+4<length && !strncmp(html+i,"&amp;",5)) {c='&';i+=4;}
            else if(i+3<length && !strncmp(html+i,"&lt;",4)) {c='<';i+=3;}
            else if(i+3<length && !strncmp(html+i,"&gt;",4)) {c='>';i+=3;}
            else if(i+5<length && !strncmp(html+i,"&quot;",6)) {c='"';i+=5;}
            else if(i+5<length && !strncmp(html+i,"&nbsp;",6)) {c=' ';i+=5;}
        }
        if(c=='\r'||c=='\n'||c=='\t'||c==' ') {
            if(!space && out){browserContent_[out++]=' ';space=true;}
        } else if((unsigned char)c>=32) {
            browserContent_[out++]=c;space=false;
        }
    }
    browserContent_[out]=0;
    browserLoading_=false;
    BrowserStatus("HTTP 200 OK - text extracted");
}
void Desktop::DragDamage(int oldX,int oldY,int newX,int newY,int w,int h)
{
    int x0=oldX<newX?oldX:newX;
    int y0=oldY<newY?oldY:newY;
    int x1=(oldX>newX?oldX:newX)+w+8;
    int y1=(oldY>newY?oldY:newY)+h+10;
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>width_)x1=width_;
    if(y1>height_)y1=height_;
    if(!dirty_) {
        damagePartial_=true;
        damageX0_=x0;damageY0_=y0;damageX1_=x1;damageY1_=y1;
    }else if(damagePartial_) {
        if(x0<damageX0_)damageX0_=x0;
        if(y0<damageY0_)damageY0_=y0;
        if(x1>damageX1_)damageX1_=x1;
        if(y1>damageY1_)damageY1_=y1;
    }
    dirty_=true;
}
void Desktop::GetDamage(int &x,int &y,int &w,int &h) const
{
    if(!damagePartial_) {x=0;y=0;w=width_;h=height_;return;}
    x=damageX0_;y=damageY0_;
    w=damageX1_-damageX0_;h=damageY1_-damageY0_;
}
void Desktop::ClearDamage() {damagePartial_=false;}

void Desktop::Print(const char *s)
{
    unsigned n=(unsigned)strlen(s);
    if (n>sizeof(terminal_)-1) {s+=n-(sizeof(terminal_)-1);n=sizeof(terminal_)-1;}
    if (termLen_+n>=sizeof(terminal_)) {
        unsigned remove=termLen_+n-(sizeof(terminal_)-1);
        memmove(terminal_,terminal_+remove,termLen_-remove);
        termLen_-=remove;
    }
    memcpy(terminal_+termLen_,s,n); termLen_+=n; terminal_[termLen_]=0;
    dirty_=true;
}
void Desktop::Prompt() { Print("xdos> "); }

void Desktop::Open(int i)
{ if(i<0||i>=NWIN)return;windows_[i].open=true;active_=i;dirty_=true; }

int Desktop::HitWindow(int x,int y) const
{
    if(active_>=0 && xdos_arm_hit_window(&windows_[active_], x, y)) return active_;
    for(int i=NWIN-1;i>=0;--i) if(i!=active_&&
        xdos_arm_hit_window(&windows_[i], x, y)) return i;
    return -1;
}

void Desktop::PaintAt(int x,int y)
{
    Window &w=windows_[Paint];
    if (!Contains(x,y,w.x+14,w.y+63,w.w-28,w.h-80)) return;
    if (markCount_>=NPAINT) { // discard earliest samples
        memmove(marks_,marks_+250,(NPAINT-250)*sizeof(Mark));markCount_-=250;
    }
    marks_[markCount_++]={(unsigned short)(x-w.x-14),(unsigned short)(y-w.y-63)};
    dirty_=true;
}

void Desktop::TapCalculator(int x,int y)
{
    Window &w=windows_[Calculator];
    int sx=w.x+18, sy=w.y+106;
    if (!Contains(x,y,sx,sy,4*76,4*61)) return;
    int c=(x-sx)/76,r=(y-sy)/61;
    static const char keys[4][5]={"789/","456*","123-","C0=+"};
    char k=keys[r][c];
    if(k>='0'&&k<='9') {
        if(calcNewEntry_) {calcEntry_=0;calcNewEntry_=false;}
        if(calcEntry_<100000000)calcEntry_=calcEntry_*10+(k-'0');
        LongToString(calcEntry_,calcDisplay_);
    } else if(k=='C') {
        calcValue_=calcEntry_=0;calcOp_=0;calcNewEntry_=false;strcpy(calcDisplay_,"0");
    } else if(k=='=' && calcOp_) {
        if(calcOp_=='+')calcValue_+=calcEntry_;
        if(calcOp_=='-')calcValue_-=calcEntry_;
        if(calcOp_=='*')calcValue_*=calcEntry_;
        if(calcOp_=='/' && calcEntry_!=0)calcValue_/=calcEntry_;
        if(calcOp_=='/' && calcEntry_==0) {strcpy(calcDisplay_,"DIV BY ZERO");calcOp_=0;dirty_=true;return;}
        calcEntry_=calcValue_;calcOp_=0;calcNewEntry_=true;
        LongToString(calcValue_,calcDisplay_);
    } else if(k!='=') {
        calcValue_=calcEntry_;calcOp_=k;calcNewEntry_=true;
    }
    dirty_=true;
}

void Desktop::Mouse(MouseAction action,unsigned buttons,int x,int y)
{
    mouseX_=x;mouseY_=y;
    if(action==Up) {dragIndex_=-1;return;}
    if(action==Move) {
        if(dragIndex_>=0 && (buttons&1)) {
            Window &w=windows_[dragIndex_];
            int ox=w.x, oy=w.y;
            if(xdos_arm_drag_window(&w, x, y, dragDX_, dragDY_,
                                    width_-w.w, height_-w.h-41))
                DragDamage(ox,oy,w.x,w.y,w.w,w.h);
        } else if((buttons&1) && active_==Paint && windows_[Paint].open) PaintAt(x,y);
        return;
    }
    if(action!=Down||!(buttons&1)) return;
    int i=HitWindow(x,y);
    if(i>=0) {
        Window &w=windows_[i];active_=i;dirty_=true;
        if (Contains(x,y,w.x+w.w-36,w.y+5,29,25)) {w.open=false;active_=-1;for(int z=0;z<NWIN;z++)if(windows_[z].open)active_=z;return;}
        if (Contains(x,y,w.x,w.y,w.w,34)) {dragIndex_=i;dragDX_=x-w.x;dragDY_=y-w.y;return;}
        if(i==Browser && browserLoading_ &&
           Contains(x,y,w.x+w.w-89,w.y+85,76,25)) {
            CancelBrowser();
            return;
        }
        if(i==Calculator) TapCalculator(x,y);
        if(i==Paint) PaintAt(x,y);
        if(i==Browser && !browserLoading_ && Contains(x,y,w.x+12,w.y+45,w.w-24,34)) {
            browserURL_[0]=0;BrowserStatus("Enter URL, then press Enter");
        }
        return;
    }
    // Taskbar app buttons
    if(y>=height_-48) {
        for(int j=0;j<NWIN;j++) if(Contains(x,y,156+j*94,height_-44,88,38)){Open(j);return;}
        return;
    }
    // Desktop shortcuts
    for(int j=0;j<NWIN;j++) if(Contains(x,y,20,97+j*80,98,72)){Open(j);return;}
}

void Desktop::Key(char ch)
{
    if(active_!=Terminal && active_!=Notepad && active_!=Browser)return;
    if(active_==Browser) {
        if (ch==27) { CancelBrowser(); return; } // Escape
        unsigned n=(unsigned)strlen(browserURL_);
        if(ch=='\b'||ch==127) {if(n)browserURL_[n-1]=0;}
        else if(ch=='\r'||ch=='\n') SubmitBrowserURL();
        else if(ch>=33 && ch<=126 && n<sizeof(browserURL_)-1 && !browserLoading_) {
            browserURL_[n]=ch;browserURL_[n+1]=0;
        }
        dirty_=true;return;
    }
    if(active_==Notepad) {
        if(ch=='\b'||ch==127) {if(noteLen_>0)note_[--noteLen_]=0;}
        else if((ch>=32 && ch<=126) || ch=='\n'||ch=='\r') {
            if(noteLen_<sizeof(note_)-1) {note_[noteLen_++]=ch=='\r'?'\n':ch;note_[noteLen_]=0;}
        }
        dirty_=true;return;
    }
    if(ch=='\b'||ch==127) {
        if(cmdLen_>0){cmdLen_--;cmd_[cmdLen_]=0;if(termLen_>0){terminal_[--termLen_]=0;}}
    } else if(ch=='\r'||ch=='\n') {
        Print("\n");cmd_[cmdLen_]=0;
        if(!strcmp(cmd_,"help")) Print("help about clear ver ls net browse <http://url> stop echo <text>\n");
        else if(!strcmp(cmd_,"clear")) {termLen_=0;terminal_[0]=0;}
        else if(!strcmp(cmd_,"about")||!strcmp(cmd_,"ver")) Print("XD OS ARM32 V0.4 RECONSTRUCTED / Circle / Pi 3B\n");
        else if(!strcmp(cmd_,"net")) {Print(netStatus_);Print("\n");}
        else if(!strcmp(cmd_,"stop")) {CancelBrowser();Print("Browser cancelled.\n");}
        else if(!strncmp(cmd_,"browse ",7)) {
            strncpy(browserURL_,cmd_+7,sizeof(browserURL_)-1);
            browserURL_[sizeof(browserURL_)-1]=0;
            Open(Browser);SubmitBrowserURL();
        }
        else if(!strcmp(cmd_,"ls")) Print("Storage driver not integrated yet.\n");
        else if(!strncmp(cmd_,"echo ",5)) {Print(cmd_+5);Print("\n");}
        else if(cmdLen_>0) Print("Unknown command. Type help.\n");
        cmdLen_=0;cmd_[0]=0;Prompt();
    } else if(ch>=32 && ch<=126 && cmdLen_<sizeof(cmd_)-1) {
        cmd_[cmdLen_++]=ch;cmd_[cmdLen_]=0;
        char s[2]={ch,0};Print(s);
    }
    dirty_=true;
}

void Desktop::Fill(int x,int y,int w,int h,unsigned color)
{
    // Translation of the original x86 rect + clip to AArch32 assembly.
    // Addressing honors the real GPU pitch, including padding.
    xdos_arm_rect(framebuffer_, pitchBytes_, width_, height_,
                  x, y, w, h, color, depth_);
}
void Desktop::Frame(int x,int y,int w,int h,unsigned color)
{Fill(x,y,w,1,color);Fill(x,y+h-1,w,1,color);Fill(x,y,1,h,color);Fill(x+w-1,y,1,h,color);}
void Desktop::Text(int x,int y,const char *s,unsigned color)
{
    // Source font is still decoded by Circle's known-good Font8x16 helper.
    // Glyph rasterization / clipping / pixel writes run in ARM assembly.
    while(*s) {
        unsigned char c=(unsigned char)*s++;
        if(c<32||c>126)c='?';
        unsigned short rows[16];
        for(unsigned yy=0;yy<16;yy++) {
            CCharGenerator::TPixelLine line=font.GetPixelLine((char)c,yy);
            unsigned short mask=0;
            for(unsigned xx=0;xx<8;xx++)
                if(font.GetPixel(xx,line)) mask|=(unsigned short)(1u<<(7-xx));
            rows[yy]=mask;
        }
        xdos_arm_glyph(framebuffer_,pitchBytes_,width_,height_,
                       x,y,rows,color,depth_);
        x+=8;
    }
}
void Desktop::TextLines(int x,int y,int right,int bottom,const char *s,unsigned color,int maxCols)
{
    const int left=x;
    int count=0;
    if(maxCols<=0)maxCols=(right-left)/8;
    for(;*s && y+16<=bottom;s++) {
        if(*s=='\r')continue;
        if(*s=='\n'||count>=maxCols){x=left;y+=17;count=0;if(*s=='\n')continue;}
        if(y+16>bottom)break;
        char one[2]={*s,0};Text(x,y,one,color);x+=8;count++;
    }
}
void Desktop::DrawSmallIcon(int i,int x,int y)
{
    static const unsigned colors[]={0x1E8E86,0x4669B2,0xE99B45,0xCE6A9A,0x5A83BB,0x76B9D5};
    Fill(x,y,34,32,colors[i]);Frame(x,y,34,32,0xC0D4EA);
    Text(x+13,y+8,icons[i],WHITE);
}
void Desktop::DrawIcons()
{
    for(int i=0;i<NWIN;i++){
        int y=97+i*80;
        DrawSmallIcon(i,29,y);
        Text(20,y+38,names[i],WHITE);
    }
}
void Desktop::DrawTaskbar()
{
    Fill(0,height_-49,width_,49,NAV);
    Fill(0,height_-49,width_,1,0x42637A);
    Fill(14,height_-42,123,33,ACCENT); Text(28,height_-33,"XD OS",NAV);
    for(int i=0;i<NWIN;i++) {
        int x=156+i*94;
        bool on=windows_[i].open;
        Fill(x,height_-44,88,38,on?(i==active_?0x4A7491:0x28445C):0x1B3247);
        if(on)Fill(x,height_-44,88,2,ACCENT);
        Text(x+9,height_-31,names[i],WHITE);
    }
    Text(width_-178,height_-30,"Pi 3B  ARM32",CYAN);
}
void Desktop::DrawDesktop()
{
    for(int y=0;y<height_-49;y++) {
        int t=y*26/(height_>1?height_:1);
        unsigned c=((unsigned)(18+t)<<16)|((unsigned)(34+t)<<8)|(unsigned)(56+t);
        Fill(0,y,width_,1,c);
    }
    Fill(0,0,width_,65,BG2);
    Fill(0,63,width_,2,ACCENT);
    Text(23,16,"XD OS",WHITE);
    Text(23,37,"ARM32 / V0.4 RECONSTRUCTED / BARE METAL",CYAN);
    Text(width_-298,20,"Raspberry Pi 3B / Circle",WHITE);
    DrawIcons();
    Text(22,height_-76,"USB KEYBOARD + MOUSE",CYAN);
    Fill(width_-248,78,228,66,0x1D3D50);
    Text(width_-236,85,"USB / NETWORK",CYAN);
    Text(width_-236,103,keyboard_?"KEY OK":"KEY --",keyboard_?ACCENT:RED);
    Text(width_-144,103,mouse_?"MOUSE OK":"MOUSE --",mouse_?ACCENT:RED);
    // Keep status bounded to fit the overlay (18 chars).
    char netText[19];strncpy(netText,netStatus_,18);netText[18]=0;
    Text(width_-236,122,netText,netRunning_?ACCENT:RED);
}
void Desktop::DrawTerminal(int i)
{
    Window &w=windows_[i];
    Fill(w.x+10,w.y+42,w.w-20,w.h-55,0x102232);
    int maxRows=(w.h-70)/17;
    const char *begin=terminal_;
    int lines=0;
    for(const char *p=terminal_;*p;p++)if(*p=='\n')lines++;
    int skip=lines-(maxRows-1);
    while(skip>0&&*begin){if(*begin++=='\n')skip--;}
    TextLines(w.x+21,w.y+53,w.x+w.w-15,w.y+w.h-16,begin,0x8AF2C3,(w.w-50)/8);
}
void Desktop::DrawNotepad(int i)
{
    Window &w=windows_[i];
    Fill(w.x+11,w.y+44,w.w-22,w.h-56,WHITE);
    Text(w.x+24,w.y+53,"Untitled - unsaved",DIM);
    Fill(w.x+22,w.y+75,w.w-44,1,FRAME);
    TextLines(w.x+24,w.y+86,w.x+w.w-20,w.y+w.h-15,note_,INK,(w.w-54)/8);
    if(noteLen_==0)Text(w.x+24,w.y+86,"Click here and type...",DIM);
}
void Desktop::DrawCalculator(int i)
{
    Window &w=windows_[i];
    Fill(w.x+16,w.y+45,w.w-32,48,0xE4EFF6);
    Text(w.x+28,w.y+61,calcDisplay_,INK);
    const char* rows[]={"789/","456*","123-","C0=+"};
    for(int r=0;r<4;r++)for(int c=0;c<4;c++) {
        int x=w.x+18+c*76,y=w.y+106+r*61;
        unsigned bg=c==3?0xD7E9EB:0xECF1F6;
        Fill(x,y,69,54,bg);Frame(x,y,69,54,FRAME);
        char label[2]={rows[r][c],0};Text(x+30,y+18,label,INK);
    }
}
void Desktop::DrawPaint(int i)
{
    Window &w=windows_[i];
    Text(w.x+18,w.y+42,"Left drag to draw; reopen to continue",DIM);
    Fill(w.x+14,w.y+63,w.w-28,w.h-80,WHITE);
    Frame(w.x+14,w.y+63,w.w-28,w.h-80,FRAME);
    for(unsigned j=0;j<markCount_;j++) {
        int px=w.x+14+(int)marks_[j].x,py=w.y+63+(int)marks_[j].y;
        Fill(px-2,py-2,5,5,0x178C84);
    }
}
void Desktop::DrawBrowser(int i)
{
    Window &w=windows_[i];
    Fill(w.x+12,w.y+44,w.w-24,36,0xE6EDF4);
    Frame(w.x+12,w.y+44,w.w-24,36,FRAME);
    Text(w.x+20,w.y+55,"URL:",INK);
    int maxChars=(w.w-90)/8;
    if(maxChars<1)maxChars=1;
    char shown[100];
    int len=(int)strlen(browserURL_);
    const char *from=browserURL_;
    if(len>maxChars) from+=len-maxChars;
    int n=(int)strlen(from);if(n>maxChars)n=maxChars;
    if(n>99)n=99;
    memcpy(shown,from,n);shown[n]=0;
    Text(w.x+60,w.y+55,shown,INK);
    char status[80];
    strncpy(status,browserStatus_,sizeof(status)-1);status[sizeof(status)-1]=0;
    // Reserve the right side for the Stop button during a request.
    if(browserLoading_) {
        status[48]=0;
        Fill(w.x+w.w-89,w.y+85,76,25,RED);
        Text(w.x+w.w-77,w.y+90,"STOP",WHITE);
    }
    Text(w.x+18,w.y+91,status,0x22538C);
    Fill(w.x+12,w.y+116,w.w-24,w.h-132,WHITE);
    TextLines(w.x+22,w.y+127,w.x+w.w-20,w.y+w.h-13,
              browserContent_,INK,(w.w-52)/8);
}

void Desktop::DrawPlaceholder(int i)
{
    Window &w=windows_[i];
    const char *msg = i==Files?"FAT32 filesystem not integrated yet":"Network and browser are not integrated yet";
    Text(w.x+23,w.y+63,msg,INK);
    Text(w.x+23,w.y+98,"Future XD OS ARM32 milestone",DIM);
}
void Desktop::DrawWindow(int i)
{
    Window &w=windows_[i];
    Fill(w.x+5,w.y+7,w.w,w.h,0x071522);
    Fill(w.x,w.y,w.w,w.h,SURFACE);
    Frame(w.x,w.y,w.w,w.h,FRAME);
    Fill(w.x,w.y,w.w,35,i==active_?BAR:0x52687C);
    Fill(w.x,w.y,w.w,2,i==active_?ACCENT:0x89B2BE);
    Text(w.x+14,w.y+11,names[i],WHITE);
    Fill(w.x+w.w-36,w.y+5,29,25,RED);
    Text(w.x+w.w-25,w.y+10,"X",WHITE);
    switch(i){case Terminal:DrawTerminal(i);break;case Notepad:DrawNotepad(i);break;
        case Calculator:DrawCalculator(i);break;case Paint:DrawPaint(i);break;
        case Browser:DrawBrowser(i);break;
        default:DrawPlaceholder(i);}
}
void Desktop::Render(void *fb,unsigned pitchBytes,unsigned depthBits)
{
    if(!fb || (depthBits!=16 && depthBits!=32) || pitchBytes==0)return;
    depth_=depthBits;
    framebuffer_=fb;
    pitchBytes_=pitchBytes;
    DrawDesktop();
    for(int i=0;i<NWIN;i++)if(i!=active_&&windows_[i].open)DrawWindow(i);
    if(active_>=0&&windows_[active_].open)DrawWindow(active_);
    DrawTaskbar();
    dirty_=false;
}
