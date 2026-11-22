#include "pixel_bbs.h"
#include "desktop_apps.h"

namespace {
char DesktopAppInitial(const char *name)
{
    for (; *name; ++name) {
        if (*name>='A' && *name<='Z') return *name;
        if (*name>='a' && *name<='z') return char('A'+(*name-'a'));
    }
    return '?';
}

unsigned DesktopAppColor(char initial)
{
    // A-Z. Assign broadly separated hues to common initials (E, T, A, O, I, N, S).
    constexpr unsigned palette[]={
        0x007361,0x676125,0x1F6490,0x5E3979,0xA7303C,0x873851,0x63692C,
        0x226680,0x6F3D9E,0x4D4789,0xAE415A,0x5E6725,0x92442C,0x475F1F,
        0x965324,0x6B4B9A,0x6F5024,0x9D365B,0x225AA3,0x7B4E1E,0x87401F,
        0x126D74,0x963F71,0x33608E,0x006758,0x70376D,
    };
    return initial>='A' && initial<='Z' ? palette[initial-'A'] : 0x4B5566;
}

PixelBbs::Shortcut PlaceDiscoveredApp(int width,int height,unsigned index)
{
    constexpr int columns=XdosDesktopColumns;
    constexpr int rows=2;
    const int reference=width*3/4<height ? width*3/4 : height;
    const int scale=reference<480 ? 480 : reference>1200 ? 1200 : reference;
    const int margin=20*scale/768;
    const int spare=(width-columns*128)/2;
    const int sideMargin=spare<0 ? 0 : spare<margin ? spare : margin;
    const int top=80+margin;
    const int icon=34*scale/768;
    const int gap=7*scale/768;
    const int textHeight=width>=1280 && height>=900 ? 32 : 16;
    const int cellHeight=icon+gap+textHeight;
    const int columnWidth=(width-2*sideMargin)/columns;
    const int rowHeight=(height-49-top-margin)/rows;
    const int x=sideMargin+int(index%columns)*columnWidth;
    const int y=top+int(index/columns)*rowHeight+(rowHeight-cellHeight)/2;
    return {x,y,columnWidth,cellHeight,icon,gap};
}
}

#include "desktop.h"
#include "xdos_text_keys.h"
#include <stdint.h>
#include "asm_gui.h"
#include <circle/chargenerator.h>
#include <circle/font.h>
#include <string.h>
#include <strings.h>

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

Desktop::Desktop() :
    netRunning_(false),browserPending_(false),browserLoading_(false),
    browserRequestID_(0),
    damagePartial_(false),damageX0_(0),damageY0_(0),damageX1_(0),damageY1_(0),
    termLen_(0),noteLen_(0),cmdLen_(0),
    width_(1024),height_(768),active_(-1),dragIndex_(-1),dragDX_(0),dragDY_(0),
    mouseX_(0),mouseY_(0),dirty_(true),keyboard_(false),mouse_(false),lvglRequested_(false),appLaunchPending_(false),noteRevision_(0),terminalRevision_(0),networkRevision_(0),
    framebuffer_(0),pitchBytes_(0),depth_(0)
{
    windows_[0]={152,120,626,380,false};
    windows_[1]={440,250,468,360,false};
    windows_[2]={280,185,360,420,false};
    windows_[3]={220,155,620,440,false};
    windows_[4]={260,170,500,350,false};
    windows_[5]={300,200,520,340,false};
    terminal_[0]=note_[0]=cmd_[0]='\0';
    strcpy(browserURL_,"http://example.com/");
    strcpy(browserContent_,"Enter an HTTP URL and press Enter to load.\nHTTPS is not yet supported.\n");
    strcpy(browserStatus_,"Ready - HTTP only");
    strcpy(netStatus_,"NET: AFTER DESKTOP");
    strcpy(cpuStatus_,"CPU: WFI initializing");
    strcpy(storageStatus_,"FAT32: INITIALIZING");
    strcpy(usbStorageStatus_,"USB: WAIT DEVICE");
    usbStorageReady_=false;
    usbStorageRevision_=0;
    strcpy(filesContent_,"FAT32 storage not mounted yet.\n");
    strcpy(noteStatus_,"SD:/XDOS/NOTE.TXT");
    storageReady_=storagePending_=storageBusy_=false;
    memset(&storageRequest_,0,sizeof(storageRequest_));
    Print("XD OS ARM32 V0.6.2 RC3 | Raspberry Pi 3 bare-metal\n");
    Print("Circle USB / LVGL 9 / FAT32 / deferred ETH/WIFI\n");
    Print("Type help then Enter to see commands.\n\n");
    Prompt();
}

void Desktop::Resize(int w,int h)
{
    width_=w; height_=h; RequestFullRepaint();
    for (int i=0;i<NWIN;i++) {
        if (windows_[i].w>width_-20) windows_[i].w=width_-20;
        if (windows_[i].h>height_-65) windows_[i].h=height_-65;
        windows_[i].x=Clamp(windows_[i].x,0,width_-windows_[i].w);
        windows_[i].y=Clamp(windows_[i].y,0,height_-windows_[i].h-42);
    }
}

void Desktop::SetUSB(bool keyboard,bool mouse)
{
    if (keyboard_!=keyboard || mouse_!=mouse) MarkDamage(width_-248,78,width_-20,182);
    keyboard_=keyboard;
    mouse_=mouse;
}

void Desktop::SetNetwork(bool running,const char *status)
{
    if(!status)status="NETWORK ERROR";
    if(netRunning_!=running || strcmp(netStatus_,status)) {
        ++networkRevision_;
        netRunning_=running;
        strncpy(netStatus_,status,sizeof(netStatus_)-1);
        netStatus_[sizeof(netStatus_)-1]=0;
        MarkDamage(width_-248,78,width_-20,182);
    }
}
void Desktop::SetCpuLoad(unsigned cpu0,unsigned cpu1,unsigned cpu2,unsigned cpu3)
{
    const unsigned v[4]={cpu0>100?100:cpu0,cpu1>100?100:cpu1,cpu2>100?100:cpu2,cpu3>100?100:cpu3};
    if (!memcmp(v,cpuLoad_,sizeof v)) return;
    memcpy(cpuLoad_,v,sizeof v);
    MarkDamage(width_-248,140,width_-20,182);
    MarkDamage(CoreStripX,height_-49,width_,height_);
}
void Desktop::SetCoreApps(const char (*apps)[64])
{
    if (!memcmp(apps,coreApps_,sizeof coreApps_)) return;
    memcpy(coreApps_,apps,sizeof coreApps_);
    MarkDamage(CoreStripX,height_-49,width_,height_);
}
void Desktop::SetStorage(bool ready,const char *status)
{
    if(!status) status="FAT32: ERROR";
    storageReady_=ready;
    strncpy(storageStatus_,status,sizeof(storageStatus_)-1);
    storageStatus_[sizeof(storageStatus_)-1]=0;
    if(!ready) strncpy(filesContent_,"FAT32 volume not available.\n",sizeof(filesContent_)-1);
    if (windows_[Files].open) RequestFullRepaint();
    else MarkDamage(22,height_-95,width_,height_-79);
}

void Desktop::SetUSBStorage(bool ready,const char *status)
{
    if(!status)status="USB: UNKNOWN";
    if(usbStorageReady_ != ready || strcmp(usbStorageStatus_,status)) {
        usbStorageReady_=ready;
        strncpy(usbStorageStatus_,status,sizeof(usbStorageStatus_)-1);
        usbStorageStatus_[sizeof(usbStorageStatus_)-1]=0;
        ++usbStorageRevision_;
        RequestFullRepaint();
    }
}

bool Desktop::QueueStorage(StorageAction action,const char *name,const char *text)
{
    const bool usbAction=action>=StorageUSBList && action<=StorageUSBMount;
    const bool requiresUSB=usbAction &&
        action!=StorageUSBMount && action!=StorageUSBEject;
    if(requiresUSB && !usbStorageReady_) {
        Print("USB not mounted. Use 'usb status' or 'usb mount'.\n");
        return false;
    }
    if(!usbAction && !storageReady_) {
        Print("FAT32 SD not ready. Check storage status.\n");
        return false;
    }
    if(storageBusy_||storagePending_) {
        Print("FAT32: previous operation still running.\n");
        return false;
    }
    StorageRequest &r=storageRequest_;
    memset(&r,0,sizeof(r));
    r.action=action;
    if(name) {
        strncpy(r.filename,name,sizeof(r.filename)-1);
        r.filename[sizeof(r.filename)-1]=0;
    }
    if(action==StorageSaveNote) {
        r.length=noteLen_;
        if(r.length>=sizeof(r.data))r.length=sizeof(r.data)-1;
        memcpy(r.data,note_,r.length);
    }
    if(action==StorageUSBWrite) {
        if(!text || !*text || strlen(text)>120) {
            Print("usb write: text must be 1..120 characters.\n");
            return false;
        }
        r.length=(unsigned)strlen(text);
        memcpy(r.data,text,r.length);
    }
    storagePending_=true;
    storageBusy_=true;
    if(usbAction) {
        ++usbStorageRevision_;
        strncpy(usbStorageStatus_,"USB: BUSY",sizeof(usbStorageStatus_)-1);
        usbStorageStatus_[sizeof(usbStorageStatus_)-1]=0;
    } else {
        strncpy(storageStatus_,"FAT32: BUSY",sizeof(storageStatus_)-1);
        storageStatus_[sizeof(storageStatus_)-1]=0;
    }
    RequestFullRepaint();
    return true;
}

bool Desktop::TakeStorageRequest(StorageRequest &out)
{
    if(!storagePending_)return false;
    out=storageRequest_;
    storagePending_=false;
    return true;
}

void Desktop::StorageCompleted(StorageAction action,bool success,const char *message,
                               const char *data,unsigned length)
{
    storageBusy_=false;
    if(!message)message=success?"OK":"ERROR";
    const bool usbAction=action>=StorageUSBList && action<=StorageUSBMount;
    if(usbAction) {
        // Preserve an explicit mount/eject/error status emitted by worker,
        // but clear the transient USB BUSY banner after ordinary commands.
        if(!strcmp(usbStorageStatus_,"USB: BUSY")) {
            strncpy(usbStorageStatus_,usbStorageReady_?
                    "USB: FAT MOUNTED (USB:/)":"USB: NOT MOUNTED",
                    sizeof(usbStorageStatus_)-1);
            usbStorageStatus_[sizeof(usbStorageStatus_)-1]=0;
        }
        ++usbStorageRevision_;
    } else {
        strncpy(storageStatus_,success?"FAT32: READY":"FAT32: ERROR",sizeof(storageStatus_)-1);
        storageStatus_[sizeof(storageStatus_)-1]=0;
    }
    if(action==StorageSaveNote || action==StorageLoadNote) {
        ++noteStorageRevision_;
        strncpy(noteStatus_,message,sizeof(noteStatus_)-1);
        noteStatus_[sizeof(noteStatus_)-1]=0;
        if(action==StorageLoadNote && success && data) {
            if(length>=sizeof(note_))length=sizeof(note_)-1;
            memcpy(note_,data,length);note_[length]=0;noteLen_=length;
        }
    }
    if(action==StorageLoadNote && success) ++noteRevision_;
    if (action == StorageList || action == StorageListRoot || action == StorageUSBList ||
        action == StorageCat || action == StorageUSBCat) {
        strncpy(filesStatus_,message,sizeof filesStatus_-1);
        filesStatus_[sizeof filesStatus_-1] = 0;
        ++filesRevision_;
        if (action == StorageCat || action == StorageUSBCat) {
            if (success && data) {
                const unsigned n = length < sizeof filesContent_-1 ? length : sizeof filesContent_-1;
                memcpy(filesContent_,data,n);
                filesContent_[n] = 0;
            } else filesContent_[0] = 0;
        }
    }
    if((action==StorageList||action==StorageListRoot||action==StorageUSBList) && success && data) {
        if(length>=sizeof(filesContent_))length=sizeof(filesContent_)-1;
        memcpy(filesContent_,data,length);filesContent_[length]=0;
        Print(filesContent_); // Terminal 'ls' and Files window share the same listing.
    }
    if((action==StorageCat||action==StorageUSBCat) && success && data) {
        char limited[4096];
        if(length>=sizeof(limited))length=sizeof(limited)-1;
        memcpy(limited,data,length);limited[length]=0;
        Print(limited);
        if(length && limited[length-1]!='\n')Print("\n");
    }
    Print(message);Print("\n");
    RequestFullRepaint();
}

bool Desktop::RequestHttp(const char *url)
{
    if (!url || !*url || strlen(url) >= sizeof(browserURL_) || browserLoading_) return false;
    strcpy(browserURL_,url);
    SubmitBrowserURL();
    return true;
}

void Desktop::BrowserStatus(const char *message)
{
    ++browserRevision_;
    if(!message)message="";
    strncpy(browserStatus_,message,sizeof(browserStatus_)-1);
    browserStatus_[sizeof(browserStatus_)-1]=0;
    RequestFullRepaint();
}
void Desktop::BrowserError(const char *message)
{
    browserLoading_=false;
    BrowserStatus(message);
    strncpy(browserContent_,message,sizeof(browserContent_)-1);
    browserContent_[sizeof(browserContent_)-1]=0;
    RequestFullRepaint();
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
void Desktop::BrowserLoaded(const char *text,unsigned length)
{
    const unsigned size = length < sizeof(browserContent_)-1 ? length : sizeof(browserContent_)-1;
    memcpy(browserContent_,text,size);
    browserContent_[size]=0;
    browserLoading_=false;
    BrowserStatus("HTTP 200 OK - text extracted");
}
void Desktop::DragDamage(int oldX,int oldY,int newX,int newY,int w,int h)
{
    int x0=oldX<newX?oldX:newX;
    int y0=oldY<newY?oldY:newY;
    int x1=(oldX>newX?oldX:newX)+w+8;
    int y1=(oldY>newY?oldY:newY)+h+10;
    MarkDamage(x0,y0,x1,y1);
}
void Desktop::MarkDamage(int x0,int y0,int x1,int y1)
{
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>width_)x1=width_;
    if(y1>height_)y1=height_;
    if(x0>=x1 || y0>=y1)return;
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
    if (!s || !*s) return;
    ++terminalRevision_;
    unsigned n=(unsigned)strlen(s);
    if (n>sizeof(terminal_)-1) {s+=n-(sizeof(terminal_)-1);n=sizeof(terminal_)-1;}
    if (termLen_+n>=sizeof(terminal_)) {
        unsigned remove=termLen_+n-(sizeof(terminal_)-1);
        memmove(terminal_,terminal_+remove,termLen_-remove);
        termLen_-=remove;
    }
    memcpy(terminal_+termLen_,s,n); termLen_+=n; terminal_[termLen_]=0;
    RequestFullRepaint();
}
void Desktop::Prompt() { Print("xdos> "); }

bool Desktop::TakeCalculatorRequest()
{
    const bool pending = calculatorPending_;
    calculatorPending_ = false;
    return pending;
}

bool Desktop::TakeNotepadRequest()
{
    const bool pending = notepadPending_;
    notepadPending_ = false;
    return pending;
}

Desktop::Workspace Desktop::TakeWorkspaceRequest()
{
    const auto request = workspacePending_;
    workspacePending_ = NoWorkspace;
    return request;
}

void Desktop::Open(int i)
{
    if (i == Terminal) {
        workspacePending_ = TerminalApp;
        return;
    }
    if (i == Paint || i == Files || i == Browser) {
        workspacePending_ = i == Paint ? PaintApp : (i == Files ? FilesApp : BrowserApp);
        return;
    }
    if (i == Notepad) {
        notepadPending_ = true;
        return;
    }
    if (i == Calculator) {
        calculatorPending_ = true;
        return;
    }
    if(i<0||i>=NWIN)return;
    windows_[i].open=true;active_=i;RequestFullRepaint();
}

int Desktop::HitWindow(int x,int y) const
{
    if(active_>=0 && xdos_arm_hit_window(&windows_[active_], x, y)) return active_;
    for(int i=NWIN-1;i>=0;--i) if(i!=active_&&
        xdos_arm_hit_window(&windows_[i], x, y)) return i;
    return -1;
}



namespace {
constexpr int PowerMenuX = 14;

struct PowerMenuLayout {
    int top, width, row, scale;
};

PowerMenuLayout GetPowerMenuLayout(int width,int height)
{
    const int scale=width>=1440 && height>=900 ? 3 : width>=640 && height>=400 ? 2 : 1;
    const int row=22*scale;
    return {height-49-4*scale-2*row,112*scale,row,scale};
}
}

void Desktop::DrawPowerText(int x,int y,const char *text,int scale)
{
    for (; *text; ++text,x+=8*scale) {
        for (int row=0;row<16;++row) {
            const auto bits=font.GetPixelLine(*text,row);
            for (int column=0;column<8;++column) {
                if (font.GetPixel(column,bits))
                    Fill(x+column*scale,y+row*scale,scale,scale,WHITE);
            }
        }
    }
}

int Desktop::IconHighlightSize() const
{
    const auto slot=PlaceDiscoveredApp(width_,height_,0);
    const int glyphScale=width_>=1280 && height_>=900 ? 2 : 1;
    int side=slot.height;
    for (unsigned i=0;i<discoveredCount_;++i) {
        const unsigned count=strlen(discoveredApps_[i].name);
        if (count>16) continue;
        int labelWidth=int(count)*8*glyphScale;
        if (labelWidth>slot.width) labelWidth=int(count)*8;
        if (labelWidth<=slot.width && labelWidth>side) side=labelWidth;
    }
    return side<=slot.width ? side : slot.width;
}

void Desktop::IconBox(unsigned index,int &x,int &y,int &side) const
{
    const auto slot=PlaceDiscoveredApp(width_,height_,index);
    side=IconHighlightSize();
    // Never reach into the next row: rows are one pitch apart.
    const auto first=PlaceDiscoveredApp(width_,height_,0);
    const auto below=PlaceDiscoveredApp(width_,height_,XdosDesktopColumns);
    const int pitch=below.y-first.y;
    if (pitch>0 && side>pitch) side=pitch;
    if (side<slot.height) side=slot.height<=slot.width ? slot.height : slot.width;
    x=slot.x+(slot.width-side)/2;
    y=slot.y+(slot.height-side)/2;
}

void Desktop::SetIconFocus(int index)
{
    if (index==iconFocus_) return;
    const int affected[]={iconFocus_,index};
    for (const int i : affected) {
        if (i<0 || unsigned(i)>=discoveredCount_) continue;
        int x,y,side;
        IconBox(unsigned(i),x,y,side);
        MarkDamage(x,y,x+side,y+side);
    }
    iconFocus_=index;
}

void Desktop::UpdateDesktopPointer(int x,int y)
{
    if (x==pointerX_ && y==pointerY_) return;
    pointerX_=x; pointerY_=y;
    if (powerMenuOpen_) {
        const auto menu=GetPowerMenuLayout(width_,height_);
        const int row=Contains(x,y,PowerMenuX,menu.top,menu.width,2*menu.row)
            ? (y-menu.top)/menu.row : -1;
        if (row!=powerFocus_) {
            powerFocus_=row;
            MarkDamage(PowerMenuX,menu.top,PowerMenuX+menu.width,menu.top+2*menu.row);
        }
        return;
    }
    int hovered=-1;
    for (unsigned i=0;i<discoveredCount_;++i) {
        int bx,by,side;
        IconBox(i,bx,by,side);
        if (Contains(x,y,bx,by,side,side)) {
            hovered=int(i);
            break;
        }
    }
    SetIconFocus(hovered);
}

bool Desktop::DesktopNavigationKey(unsigned key)
{
    if (key==XdosKeyWindows) {
        powerMenuOpen_=!powerMenuOpen_;
        powerFocus_=-1;
        RequestFullRepaint();
        return true;
    }
    const bool enter=key=='\r' || key=='\n';
    if (powerMenuOpen_) {
        if (key==XdosKeyUp || key==XdosKeyDown) {
            powerFocus_=powerFocus_<0 ? (key==XdosKeyUp ? 1 : 0) : 1-powerFocus_;
            RequestFullRepaint();
            return true;
        }
        if (enter) {
            if (powerFocus_<0) return true;
            powerRequest_=powerFocus_==0 ? PowerRequest::Restart : PowerRequest::ShutDown;
            powerMenuOpen_=false;
            RequestFullRepaint();
            return true;
        }
        return false;
    }
    if (enter && iconFocus_>=0) {
        OpenDiscoveredApp(unsigned(iconFocus_));
        return true;
    }
    if (key!=XdosKeyLeft && key!=XdosKeyRight && key!=XdosKeyUp && key!=XdosKeyDown)
        return false;
    if (!discoveredCount_) return true;
    int index=iconFocus_;
    if (index<0) index=0;
    else if (key==XdosKeyLeft && unsigned(index)%XdosDesktopColumns>0) --index;
    else if (key==XdosKeyRight && unsigned(index)%XdosDesktopColumns<XdosDesktopColumns-1 && unsigned(index+1)<discoveredCount_) ++index;
    else if (key==XdosKeyUp && unsigned(index)>=XdosDesktopColumns) index-=XdosDesktopColumns;
    else if (key==XdosKeyDown && unsigned(index)<XdosDesktopColumns && discoveredCount_>XdosDesktopColumns)
        index=Clamp(index+XdosDesktopColumns,XdosDesktopColumns,int(discoveredCount_)-1);
    SetIconFocus(index);
    return true;
}

void Desktop::BlendWhite(int x,int y,int w,int h)
{
    const int x0=Clamp(x,clipRender_ ? clipX0_ : 0,width_);
    const int y0=Clamp(y,clipRender_ ? clipY0_ : 0,height_);
    const int x1=Clamp(x+w,0,clipRender_ ? clipX1_ : width_);
    const int y1=Clamp(y+h,0,clipRender_ ? clipY1_ : height_);
    // Blend 25% white with the wallpaper or menu surface already rendered.
    for (int row=y0;row<y1;++row) {
        auto *base=static_cast<unsigned char *>(framebuffer_)+row*pitchBytes_;
        if (depth_==16) {
            auto *pixels=reinterpret_cast<uint16_t *>(base);
            for (int col=x0;col<x1;++col) {
                const unsigned p=pixels[col], r=p>>11, g=(p>>5)&63, b=p&31;
                pixels[col]=uint16_t(((3*r+31)/4)<<11 | ((3*g+63)/4)<<5 | ((3*b+31)/4));
            }
        } else {
            auto *pixels=reinterpret_cast<uint32_t *>(base);
            for (int col=x0;col<x1;++col) {
                const unsigned p=pixels[col], r=(p>>16)&255, g=(p>>8)&255, b=p&255;
                pixels[col]=(p&0xFF000000u) | ((3*r+255)/4)<<16 |
                    ((3*g+255)/4)<<8 | ((3*b+255)/4);
            }
        }
    }
}

Desktop::PowerRequest Desktop::TakePowerRequest()
{
    const auto request=powerRequest_;
    powerRequest_=PowerRequest::None;
    return request;
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
        }
        return;
    }
    if(action!=Down||!(buttons&1)) return;
    if (Contains(x,y,14,height_-42,123,33)) {
        powerMenuOpen_=!powerMenuOpen_;
        powerFocus_=-1;
        RequestFullRepaint();
        return;
    }
    if (powerMenuOpen_) {
        const auto menu=GetPowerMenuLayout(width_,height_);
        if (Contains(x,y,PowerMenuX,menu.top,menu.width,2*menu.row))
            powerRequest_=y<menu.top+menu.row ? PowerRequest::Restart : PowerRequest::ShutDown;
        powerMenuOpen_=false;
        RequestFullRepaint();
        return;
    }
    int i=HitWindow(x,y);
    if(i>=0) {
        Window &w=windows_[i];active_=i;RequestFullRepaint();
        if (Contains(x,y,w.x+w.w-36,w.y+5,29,25)) {w.open=false;active_=-1;for(int z=0;z<NWIN;z++)if(windows_[z].open)active_=z;return;}
        if (Contains(x,y,w.x,w.y,w.w,34)) {dragIndex_=i;dragDX_=x-w.x;dragDY_=y-w.y;return;}
        if(i==Files && Contains(x,y,w.x+w.w-100,w.y+46,86,27)) {
            QueueStorage(StorageList);return;
        }
        return;
    }
    // LVGL launch always uses the FAT32 .APP manifest and .XAPP native module.
    if(y>=height_-48 && Contains(x,y,width_-303,height_-42,110,33)) {
        appLaunchPending_=true;
        return;
    }
    for (unsigned i=0; i<discoveredCount_; ++i) {
        int bx,by,side;
        IconBox(i,bx,by,side);
        if (Contains(x,y,bx,by,side,side)) {
            OpenDiscoveredApp(i);
            return;
        }
    }
}

void Desktop::RunLegacyTerminal(const char *text)
{
    cmdLen_=0;
    cmd_[0]=0;
    for (; *text; ++text) Key(*text);
    Key('\n');
}

void Desktop::Key(char ch)
{
    if (powerMenuOpen_) {
        if (ch=='r' || ch=='R') powerRequest_=PowerRequest::Restart;
        else if (ch=='s' || ch=='S') powerRequest_=PowerRequest::ShutDown;
        else if (ch!=27) return;
        powerMenuOpen_=false;
        RequestFullRepaint();
        return;
    }
#if !XDOS_USE_LVGL
    if(active_!=Terminal && active_!=Notepad && active_!=Browser)return;
#endif
    // In LVGL mode, the terminal is a dashboard pane, not a legacy Window.
    // Accept USB keyboard commands even when the ASM Terminal is not open.

    if(ch=='\b'||ch==127) {
        if(cmdLen_>0){cmdLen_--;cmd_[cmdLen_]=0;if(termLen_>0){terminal_[--termLen_]=0;}}
    } else if(ch=='\r'||ch=='\n') {
        Print("\n");cmd_[cmdLen_]=0;
        if(!strcmp(cmd_,"help")) Print("help about clear ver net browse <url> stop echo <text> lvgl lvgl builtin cpu\nSD: storage ls ls / cat <name> save load files\nUSB: usb status | usb ls | usb cat FILE | usb write FILE TEXT | usb eject | usb mount\n");
        else if(!strcmp(cmd_,"lvgl")) {appLaunchPending_=true; Print("Starting LVGL Widgets...\n");}
        else if(!strcmp(cmd_,"lvgl builtin")) {lvglRequested_=true; Print("Loading external LVGLDEMO.XAPP...\n");}
        else if(!strcmp(cmd_,"cpu")) {Print(cpuStatus_);Print("\n");}
        else if(!strcmp(cmd_,"clear")) {termLen_=0;terminal_[0]=0;}
        else if(!strcmp(cmd_,"about")||!strcmp(cmd_,"ver")) Print("XD OS ARM32 V0.6.2-RC3 / Circle / Pi 3B / USB MSC\n");
        else if(!strcmp(cmd_,"net")) {Print(netStatus_);Print("\n");}
        else if(!strcmp(cmd_,"stop")) {CancelBrowser();Print("Browser cancelled.\n");}
        else if(!strncmp(cmd_,"browse ",7)) {
            strncpy(browserURL_,cmd_+7,sizeof(browserURL_)-1);
            browserURL_[sizeof(browserURL_)-1]=0;
            workspacePending_=BrowserApp;
        }
        else if(!strcmp(cmd_,"storage")) {Print(storageStatus_);Print("\n");}
        else if(!strcmp(cmd_,"usb") || !strcmp(cmd_,"usb status")) {
            Print(usbStorageStatus_);Print("\n");
        }
        else if(!strcmp(cmd_,"usb ls") || !strcmp(cmd_,"usb dir")) QueueStorage(StorageUSBList);
        else if(!strncmp(cmd_,"usb cat ",8)) QueueStorage(StorageUSBCat,cmd_+8);
        else if(!strcmp(cmd_,"usb eject")) QueueStorage(StorageUSBEject);
        else if(!strcmp(cmd_,"usb mount")) QueueStorage(StorageUSBMount);
        else if(!strncmp(cmd_,"usb write ",10)) {
            char *filename=cmd_+10;
            char *space=strchr(filename,' ');
            if(!space || !space[1])Print("Usage: usb write FILE.TXT TEXT\n");
            else {
                *space=0;
                QueueStorage(StorageUSBWrite,filename,space+1);
            }
        }
        else if(!strcmp(cmd_,"ls") || !strcmp(cmd_,"dir")) QueueStorage(StorageList);
        else if(!strcmp(cmd_,"ls /") || !strcmp(cmd_,"dir /")) QueueStorage(StorageListRoot);
        else if(!strncmp(cmd_,"cat ",4)) QueueStorage(StorageCat,cmd_+4);
        else if(!strcmp(cmd_,"save")) QueueStorage(StorageSaveNote);
        else if(!strcmp(cmd_,"load")) QueueStorage(StorageLoadNote);
        else if(!strcmp(cmd_,"files")) Open(Files);
        else if(!strncmp(cmd_,"echo ",5)) {Print(cmd_+5);Print("\n");}
        else if(cmdLen_>0) Print("Unknown command. Type help.\n");
        cmdLen_=0;cmd_[0]=0;Prompt();
    } else if(ch>=32 && ch<=126 && cmdLen_<sizeof(cmd_)-1) {
        cmd_[cmdLen_++]=ch;cmd_[cmdLen_]=0;
        char s[2]={ch,0};Print(s);
    }
    RequestFullRepaint();
}

void Desktop::Fill(int x,int y,int w,int h,unsigned color)
{
    if (clipRender_) {
        const int right=Clamp(x+w,clipX0_,clipX1_);
        const int bottom=Clamp(y+h,clipY0_,clipY1_);
        x=Clamp(x,clipX0_,clipX1_);
        y=Clamp(y,clipY0_,clipY1_);
        w=right-x;
        h=bottom-y;
        if (w<=0 || h<=0) return;
    }
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
    if (clipRender_ && (y+16<=clipY0_ || y>=clipY1_)) return;
    while(*s) {
        if (clipRender_ && x>=clipX1_) break;
        unsigned char c=(unsigned char)*s++;
        if (clipRender_ && x+8<=clipX0_) {
            x+=8;
            continue;
        }
        if(c<32||c>126)c='?';
        unsigned short rows[16];
        for(unsigned yy=0;yy<16;yy++) {
            CCharGenerator::TPixelLine line=font.GetPixelLine((char)c,yy);
            unsigned short mask=0;
            if (!clipRender_ || (y+(int)yy>=clipY0_ && y+(int)yy<clipY1_)) {
                for(unsigned xx=0;xx<8;xx++)
                    if ((!clipRender_ || (x+(int)xx>=clipX0_ && x+(int)xx<clipX1_)) &&
                        font.GetPixel(xx,line)) mask|=(unsigned short)(1u<<(7-xx));
            }
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
void Desktop::PixelText(int x,int y,const char *label,unsigned color)
{
    if (width_<1280 || height_<900) {
        Text(x,y,label,color);
        return;
    }
    if (clipRender_ && (y+32<=clipY0_ || y>=clipY1_)) return;
    for (;*label;++label,x+=16) {
        if (clipRender_ && x>=clipX1_) break;
        if (clipRender_ && x+16<=clipX0_) continue;
        for (unsigned row=0;row<16;++row) {
            const auto bits=font.GetPixelLine(*label,row);
            for (unsigned column=0;column<8;++column)
                if (font.GetPixel(column,bits)) Fill(x+column*2,y+row*2,2,2,color);
        }
    }
}

void Desktop::ScanDesktopApps()
{
    discoveredPending_ = -1;
    iconFocus_ = -1;
    discoveredCount_ = XdosReadDesktopApps(discoveredApps_);
    RequestFullRepaint();
}

const XdosDesktopApp *Desktop::TakeDiscoveredApp()
{
    const int index = discoveredPending_;
    discoveredPending_ = -1;
    return index >= 0 ? &discoveredApps_[index] : nullptr;
}

void Desktop::OpenDiscoveredApp(unsigned index)
{
    switch (discoveredApps_[index].kind) {
    case XdosAppKind::Terminal: Open(Terminal); break;
    case XdosAppKind::Notepad: Open(Notepad); break;
    case XdosAppKind::Calculator: Open(Calculator); break;
    case XdosAppKind::Paint: Open(Paint); break;
    case XdosAppKind::Files: Open(Files); break;
    case XdosAppKind::Browser: Open(Browser); break;
    case XdosAppKind::Widgets: appLaunchPending_=true; break;
    case XdosAppKind::Benchmark: officialBenchPending_=true; break;
    case XdosAppKind::Player: workspacePending_=PlayerApp; break;
    case XdosAppKind::Music: workspacePending_=MusicApp; break;
    default: discoveredPending_=index; break;
    }
}

extern bool xdosBootIconsDrawn;
void Desktop::DrawIcons()
{
    if (discoveredCount_) xdosBootIconsDrawn = true;
    const int glyphScale=width_>=1280 && height_>=900 ? 2 : 1;
    constexpr unsigned maxLabelChars=16;
    for (unsigned i=0; i<discoveredCount_; ++i) {
        const auto slot=PlaceDiscoveredApp(width_,height_,i);
        const auto &app=discoveredApps_[i];
        const char glyph[]={DesktopAppInitial(app.name),0};
        const int centerX=slot.x+slot.width/2;
        const int iconX=centerX-slot.icon/2;
        if (int(i)==iconFocus_ && !powerMenuOpen_) {
            int bx,by,side;
            IconBox(i,bx,by,side);
            BlendWhite(bx,by,side,side);
        }
        Fill(iconX,slot.y,slot.icon,slot.icon,DesktopAppColor(glyph[0]));
        Frame(iconX,slot.y,slot.icon,slot.icon,0xC0D4EA);
        PixelText(centerX-4*glyphScale,slot.y+(slot.icon-16*glyphScale)/2,glyph,WHITE);
        const unsigned labelChars=strlen(app.name);
        if (labelChars<=maxLabelChars) {
            const int labelWidth=int(labelChars)*8;
            const int labelY=slot.y+slot.icon+slot.gap;
            if (labelWidth*glyphScale<=slot.width)
                PixelText(centerX-labelWidth*glyphScale/2,labelY,app.name,WHITE);
            else if (labelWidth<=slot.width)
                Text(centerX-labelWidth/2,labelY,app.name,WHITE);
        }
    }
}

void Desktop::DrawTaskbar()
{
    Fill(0,height_-49,width_,49,NAV);
    Fill(0,height_-49,width_,1,0x42637A);
    Fill(14,height_-42,123,33,ACCENT);
    Text(28,height_-33,"XDOS",NAV);
    // Equal columns keep the four percentages visible even with long app names.
    constexpr int glyphWidth=8;
    const int cellWidth=(width_-CoreStripX-12)/4;
    for (unsigned core=0;core<4 && cellWidth>=9*glyphWidth;++core) {
        const int x=CoreStripX+static_cast<int>(core)*cellWidth;
        char label[12]={'C','P','U',char('0'+core),' ',0};
        if (cpuLoad_[core]<=100) {
            char percent[5];
            unsigned value=cpuLoad_[core];
            unsigned size=0;
            if (value==100) { percent[size++]='1'; percent[size++]='0'; }
            else if (value>=10) percent[size++]=char('0'+value/10);
            percent[size++]=char('0'+value%10);
            percent[size++]='%';
            percent[size]=0;
            strcat(label,percent);
        } else {
            strcat(label,"--");
        }
        Text(x,height_-36,label,CYAN);
        if (!coreApps_[core][0]) continue;
        char shown[40];
        const unsigned room=static_cast<unsigned>(cellWidth/glyphWidth-1);
        const unsigned count=room<sizeof shown-1 ? room : sizeof shown-1;
        strncpy(shown,coreApps_[core],count);
        shown[count]=0;
        Text(x,height_-20,shown,WHITE);
    }
    if (powerMenuOpen_) {
        const auto menu=GetPowerMenuLayout(width_,height_);
        const int labelX=PowerMenuX+8*menu.scale;
        const int labelY=menu.top+4*menu.scale;
        Fill(PowerMenuX,menu.top,menu.width,2*menu.row,BG2);
        if (powerFocus_>=0)
            BlendWhite(PowerMenuX,menu.top+powerFocus_*menu.row,menu.width,menu.row);
        Frame(PowerMenuX,menu.top,menu.width,2*menu.row,ACCENT);
        Fill(PowerMenuX+1,menu.top+menu.row,menu.width-2,1,0x42637A);
        DrawPowerText(labelX,labelY,"Restart",menu.scale);
        Fill(labelX,labelY+16*menu.scale,8*menu.scale,menu.scale,WHITE);
        DrawPowerText(labelX,labelY+menu.row,"Shutdown",menu.scale);
        Fill(labelX,labelY+menu.row+16*menu.scale,8*menu.scale,menu.scale,WHITE);
    }
    if (shutdownNotice_) {
        const int scale=GetPowerMenuLayout(width_,height_).scale;
        const int w=224*scale, h=70*scale;
        const int x=(width_-w)/2, y=(height_-h)/2;
        Fill(x+4,y+5,w,h,0x071522);
        Fill(x,y,w,h,BG2);
        Frame(x,y,w,h,ACCENT);
        Fill(x+1,y+28*scale,w-2,1,ACCENT);
        DrawPowerText(x+8*scale,y+6*scale,"XDOS Shut Down",scale);
        DrawPowerText(x+8*scale,y+42*scale,"Safe to disconnect power.",scale);
    }
}
void Desktop::DrawDesktop()
{
    // Draw only the 8×8 cells intersecting the current damage rectangle.
    const int left=clipRender_ ? Clamp(clipX0_,0,width_) : 0;
    const int top=clipRender_ ? Clamp(clipY0_,65,height_-49) : 65;
    const int right=clipRender_ ? Clamp(clipX1_,0,width_) : width_;
    const int bottom=clipRender_ ? Clamp(clipY1_,65,height_-49) : height_-49;
    const int horizon=65+(height_-114)*3/5;
    for (int y=top&~7;y<bottom;y+=8) {
        for (int x=left&~7;x<right && left<right;x+=8) {
            const int y0=y<65 ? 65 : y;
            Fill(x,y0,8,y+8-y0,PixelBbs::Grain(x,y,y>=horizon));
        }
    }

    // Four small glyphs scale to the active screen with no bitmap allocation.
    const int cell=Clamp(width_/128,4,14);
    const int logoX=(width_-23*cell)/2;
    const int logoY=65+(height_-114-7*cell)/2;
    if (right>logoX && left<logoX+23*cell && bottom>logoY && top<logoY+8*cell) {
        for (unsigned letter=0;letter<4;++letter) {
            for (unsigned row=0;row<7;++row) {
                for (unsigned column=0;column<5;++column) {
                    if (!(PixelBbs::Logo[letter][row] & (16u>>column))) continue;
                    const int x=logoX+int(letter*6+column)*cell;
                    const int y=logoY+int(row)*cell;
                    Fill(x,y,cell-1,cell-1,PixelBbs::Ink(row));
                }
            }
        }
    }
    Fill(0,0,width_,65,BG2);
    Fill(0,63,width_,2,ACCENT);
    PixelText(23,6,"XD OS",WHITE);
    Text(23,42,"ARM32 / V0.6.2-RC2 / LVGL / WFI / FAT32",CYAN);
    Text(width_-298,20,"Raspberry Pi 3B / Circle",WHITE);
    DrawIcons();
    Text(22,height_-95,storageStatus_,storageReady_?ACCENT:RED);
    Text(22,height_-76,"USB KEYBOARD + MOUSE",CYAN);
    Fill(width_-248,78,228,104,0x1D3D50);
    Text(width_-236,85,"USB / NETWORK",CYAN);
    Text(width_-236,103,keyboard_?"KEY OK":"KEY --",keyboard_?ACCENT:RED);
    Text(width_-144,103,mouse_?"MOUSE OK":"MOUSE --",mouse_?ACCENT:RED);
    // Keep status bounded to fit the overlay (18 chars).
    char netText[19];strncpy(netText,netStatus_,18);netText[18]=0;
    Text(width_-236,122,netText,netRunning_?ACCENT:RED);
    // CPU0-3 load in the same panel, two cores per line.
    for (unsigned row=0;row<2;++row) {
        char line[24];
        char *p=line;
        for (unsigned k=0;k<2;++k) {
            const unsigned core=row*2+k;
            const unsigned v=cpuLoad_[core];
            if (k) { *p++=' '; *p++=' '; }
            *p++='C'; *p++='P'; *p++='U'; *p++=char('0'+core); *p++=' ';
            if (v>100) { *p++=' '; *p++='-'; *p++='-'; }
            else {
                *p++= v>=100 ? '1' : ' ';
                *p++= v>=10 ? char('0'+(v/10)%10) : ' ';
                *p++= char('0'+v%10);
            }
            *p++='%';
        }
        *p=0;
        Text(width_-236,141+int(row)*19,line,WHITE);
    }
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

void Desktop::DrawPlaceholder(int i)
{
    Window &w=windows_[i];
    const char *msg = i==Files?"FAT32 Files integrated":"Network and browser are not integrated yet";
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
    switch(i){case Terminal:DrawTerminal(i);break;

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

void Desktop::RenderDamage(void *fb,unsigned pitch,unsigned depth,int x,int y,int w,int h)
{
    clipX0_=Clamp(x,0,width_);
    clipY0_=Clamp(y,0,height_);
    clipX1_=Clamp(x+w,0,width_);
    clipY1_=Clamp(y+h,0,height_);
    if (clipX0_>=clipX1_ || clipY0_>=clipY1_) return;
    clipRender_=clipX0_!=0 || clipY0_!=0 || clipX1_!=width_ || clipY1_!=height_;
    Render(fb,pitch,depth);
    clipRender_=false;
}

// Optional LVGL mode; switching does not reset existing app state.
bool Desktop::TakeLVGLRequest()
{
    bool result=lvglRequested_;
    lvglRequested_=false;
    return result;
}
void Desktop::RequestFullRepaint() { dirty_=true;damagePartial_=false; }
void Desktop::SetNoteFromUI(const char *text)
{
    if(!text)return;
    strncpy(note_,text,sizeof(note_)-1);
    note_[sizeof(note_)-1]=0;
    noteLen_=(unsigned)strlen(note_);
    ++noteRevision_;
    RequestFullRepaint();
}
void Desktop::SetCPUStatus(const char *status)
{
    if(!status)return;
    if(strcmp(cpuStatus_,status)) {
        strncpy(cpuStatus_,status,sizeof(cpuStatus_)-1);
        cpuStatus_[sizeof(cpuStatus_)-1]=0;
        RequestFullRepaint();
    }
}

void Desktop::RequestLVGL() { lvglRequested_=true; }
bool Desktop::TakeAppLaunchRequest()
{
    const bool pending=appLaunchPending_;
    appLaunchPending_=false;
    return pending;
}
void Desktop::ReportAppResult(const char *message)
{
    Print(message ? message : "XAPP: unknown error");
    Print("\n");
    RequestFullRepaint();
}

bool Desktop::TakeOfficialBenchmarkLaunchRequest() {
    const bool pending = officialBenchPending_;
    officialBenchPending_ = false;
    return pending;
}


bool Desktop::OpenCatalogModule(const char *path)
{
    for (unsigned i=0;i<discoveredCount_;++i) {
        if (strcasecmp(discoveredApps_[i].path,path)) continue;
        OpenDiscoveredApp(i);
        return true;
    }
    return false;
}
