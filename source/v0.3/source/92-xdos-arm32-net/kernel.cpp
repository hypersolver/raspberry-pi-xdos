#include "kernel.h"
#include <circle/bcmframebuffer.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netconfig.h>
#include <circle/startup.h>
#include <circle/string.h>
#include <assert.h>
#include <string.h>

CKernel *CKernel::instance_=0;

CKernel::CKernel()
: screen_(1024,768), timer_(&interrupt_), logger_(options_.GetLogLevel(),&timer_),
  usb_(&interrupt_,&timer_,TRUE), net_(), webTask_(0),
  keyboard_(0), mouse_(0), keyHead_(0), keyTail_(0), mouseHead_(0), mouseTail_(0),
  debugSerialReady_(FALSE), networkInit_(FALSE), cursorVisible_(FALSE),
  back_(0),front_(0),pitch_(0),depth_(0),screenWidth_(0),screenHeight_(0),
  cursorX_(512),cursorY_(384),prevCursorX_(512),prevCursorY_(384),cursorMoved_(FALSE)
{
    instance_=this;
    actLED_.Blink(5);
}
CKernel::~CKernel() { instance_=0; }

void CKernel::Debug(const char *message)
{
    screen_.Write(message,strlen(message));
    if(debugSerialReady_)serial_.Write(message,strlen(message));
}

boolean CKernel::Initialize()
{
    debugSerialReady_=serial_.Initialize(115200);
    if(!screen_.Initialize())return FALSE;
    Debug("[V03-01] SCREEN OK\r\n");
    // Prefer UART after GUI starts, so network logs do not corrupt the desktop.
    CDevice *logDevice=debugSerialReady_ ? (CDevice*)&serial_ : (CDevice*)&screen_;
    if(!logger_.Initialize(logDevice))return FALSE;
    Debug("[V03-02] LOGGER OK\r\n");
    if(!interrupt_.Initialize()) {Debug("[E03] IRQ FAILED\r\n");return FALSE;}
    Debug("[V03-03] IRQ OK\r\n");
    if(!timer_.Initialize()) {Debug("[E04] TIMER FAILED\r\n");return FALSE;}
    Debug("[V03-04] TIMER OK\r\n");
    if(!usb_.Initialize()) {Debug("[E05] USB FAILED\r\n");return FALSE;}
    Debug("[V03-05] USB OK\r\n");

    CBcmFrameBuffer *fb=screen_.GetFrameBuffer();
    if(!fb||!fb->GetBuffer()||!fb->GetPitch())return FALSE;
    if(fb->GetDepth()!=16 && fb->GetDepth()!=32)return FALSE;
    front_=(u8*)(unsigned long)fb->GetBuffer();
    pitch_=fb->GetPitch();
    depth_=fb->GetDepth();
    screenWidth_=fb->GetWidth();
    screenHeight_=fb->GetHeight();
    if(screenHeight_>2048 || pitch_>16384 || screenWidth_>4096)return FALSE;
    const unsigned total=pitch_*screenHeight_;
    back_=new u8[total];
    if(!back_)return FALSE;
    memset(back_,0,total);
    desktop_.Resize(screenWidth_,screenHeight_);
    Debug("[V03-06] BACK BUFFER OK\r\n");
    desktop_.Render(back_,pitch_,depth_);
    memcpy(front_,back_,total);
    desktop_.ClearDamage();

    // IMPORTANT: no blocking DHCP wait; desktop must start even with no cable.
    networkInit_=net_.Initialize(FALSE);
    if(!networkInit_)desktop_.SetNetwork(false,"NET INIT FAILED");
    else desktop_.SetNetwork(false,"WAIT DHCP / LINK");
    webTask_=new CWebTask(&net_,&desktop_);
    if(!webTask_)return FALSE;
    if(debugSerialReady_)serial_.Write("[V03-07] NET TASK CREATED\r\n",strlen("[V03-07] NET TASK CREATED\r\n"));
    return TRUE;
}

void CKernel::QueueKey(char ch)
{
    unsigned h=keyHead_,next=(h+1)%QUEUE;
    if(next==keyTail_)return;
    keys_[h]=ch;__sync_synchronize();keyHead_=next;
}
void CKernel::KeyHandler(const char *text)
{
    if(!instance_||!text)return;
    while(*text)instance_->QueueKey(*text++);
}
void CKernel::QueueMouse(unsigned event,unsigned buttons,unsigned x,unsigned y)
{
    unsigned h=mouseHead_,next=(h+1)%QUEUE;
    if(next==mouseTail_)return;
    mouseEvents_[h].event=event;mouseEvents_[h].buttons=buttons;
    mouseEvents_[h].x=x;mouseEvents_[h].y=y;
    __sync_synchronize();mouseHead_=next;
}
void CKernel::MouseHandler(TMouseEvent event,unsigned buttons,unsigned x,unsigned y,int wheel)
{
    (void)wheel;
    if(instance_)instance_->QueueMouse((unsigned)event,buttons,x,y);
}
void CKernel::KeyboardRemoved(CDevice *dev,void *ctx)
{(void)dev;(void)ctx;if(instance_)instance_->keyboard_=0;}
void CKernel::MouseRemoved(CDevice *dev,void *ctx)
{(void)dev;(void)ctx;if(instance_) {instance_->mouse_=0;instance_->cursorMoved_=TRUE;}}

void CKernel::AttachDevices()
{
    if(!keyboard_) {
        CUSBKeyboardDevice *kbd=(CUSBKeyboardDevice*)names_.GetDevice("ukbd1",FALSE);
        if(kbd) {
            keyboard_=kbd;
            kbd->RegisterRemovedHandler(KeyboardRemoved);
            kbd->RegisterKeyPressedHandler(KeyHandler);
        }
    }
    if(!mouse_) {
        CMouseDevice *m=(CMouseDevice*)names_.GetDevice("mouse1",FALSE);
        if(m) {
            CBcmFrameBuffer *fb=screen_.GetFrameBuffer();
            // FALSE disables Circle's software cursor. XD OS draws one cursor
            // after buffer copy, preventing hide/show flicker on every repaint.
            if(m->Setup(fb,FALSE)) {
                mouse_=m;
                m->RegisterRemovedHandler(MouseRemoved);
                m->SetCursor(screenWidth_/2,screenHeight_/2);
                m->RegisterEventHandler(MouseHandler);
                cursorX_=screenWidth_/2;cursorY_=screenHeight_/2;
                cursorMoved_=TRUE;
            }
        }
    }
    desktop_.SetUSB(keyboard_!=0,mouse_!=0);
}

void CKernel::ProcessInput()
{
    while(keyTail_!=keyHead_) {
        unsigned t=keyTail_;char ch=keys_[t];
        __sync_synchronize();keyTail_=(t+1)%QUEUE;
        desktop_.Key(ch);
    }
    while(mouseTail_!=mouseHead_) {
        unsigned t=mouseTail_;MousePacket p;
        p.event=mouseEvents_[t].event;
        p.buttons=mouseEvents_[t].buttons;
        p.x=mouseEvents_[t].x;
        p.y=mouseEvents_[t].y;
        __sync_synchronize();mouseTail_=(t+1)%QUEUE;
        Desktop::MouseAction action;
        switch(p.event) {
        case MouseEventMouseMove:action=Desktop::Move;break;
        case MouseEventMouseDown:action=Desktop::Down;break;
        case MouseEventMouseUp:action=Desktop::Up;break;
        default:continue;
        }
        if(p.x<screenWidth_ && p.y<screenHeight_) {
            cursorX_=(int)p.x;cursorY_=(int)p.y;cursorMoved_=TRUE;
        }
        desktop_.Mouse(action,p.buttons,(int)p.x,(int)p.y);
    }
}

void CKernel::Blit(int x,int y,int w,int h)
{
    if(!back_||!front_)return;
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(x+w>(int)screenWidth_)w=(int)screenWidth_-x;
    if(y+h>(int)screenHeight_)h=(int)screenHeight_-y;
    if(w<=0||h<=0)return;
    unsigned bpp=depth_/8;
    unsigned rowBytes=(unsigned)w*bpp;
    for(int yy=y;yy<y+h;yy++) {
        unsigned offset=(unsigned)yy*pitch_+(unsigned)x*bpp;
        memcpy(front_+offset,back_+offset,rowBytes);
    }
}

void CKernel::RestoreCursor()
{
    if(cursorVisible_) {
        Blit(prevCursorX_,prevCursorY_,14,20);
        cursorVisible_=FALSE;
    }
}
void CKernel::WriteCursorPixel(int x,int y,u32 rgb)
{
    if(x<0||y<0||x>=(int)screenWidth_||y>=(int)screenHeight_)return;
    u8 *p=front_+(unsigned)y*pitch_+(unsigned)x*(depth_/8);
    if(depth_==16) {
        u16 c=(u16)(((rgb>>8)&0xF800)|((rgb>>5)&0x07E0)|((rgb>>3)&0x001F));
        memcpy(p,&c,2);
    } else {
        u32 c=0xFF000000 | rgb;
        memcpy(p,&c,4);
    }
}
void CKernel::DrawCursor()
{
    if(!mouse_)return;
    // Built-in 12x18 arrow; no separate GPU cursor and no cursor-hide flicker.
    static const u16 shape[18] = {
        0x800,0xC00,0xE00,0xF00,0xF80,0xFC0,
        0xFE0,0xFF0,0xFF8,0xFFC,0xFFE,0xFFF,
        0xFC0,0xCC0,0x8C0,0x060,0x060,0x000
    };
    for(unsigned y=0;y<18;y++)for(unsigned x=0;x<12;x++) {
        if(shape[y]&(0x800>>x)) {
            u32 color=(x==0||y==0||x==11)?0x00131E:0xFFFFFF;
            WriteCursorPixel(cursorX_+(int)x,cursorY_+(int)y,color);
        }
    }
    prevCursorX_=cursorX_;prevCursorY_=cursorY_;cursorVisible_=TRUE;
}

void CKernel::UpdateNetworkStatus()
{
    if(!networkInit_)return;
    if(!net_.IsRunning()) {
        desktop_.SetNetwork(false,"WAIT DHCP / LINK");
        return;
    }
    const u8 *ip=net_.GetConfig()->GetIPAddress()->Get();
    if(!ip)return;
    CString s;
    s.Format("%u.%u.%u.%u",(unsigned)ip[0],(unsigned)ip[1],
             (unsigned)ip[2],(unsigned)ip[3]);
    desktop_.SetNetwork(true,(const char*)s);
}

void CKernel::Run()
{
    logger_.Write("xdos",LogNotice,"XD OS ARM32 V0.3: GUI + Ethernet + HTTP browser");
    unsigned spin=0;
    while(1) {
        usb_.UpdatePlugAndPlay();
        AttachDevices();
        ProcessInput();
        if((++spin%40)==0)UpdateNetworkStatus();
        bool repaint=desktop_.NeedsRedraw();
        if(repaint||cursorMoved_) {
            RestoreCursor();
            if(repaint) {
                int x,y,w,h;
                desktop_.GetDamage(x,y,w,h);
                desktop_.Render(back_,pitch_,depth_); // ARM32 assembly rendering
                Blit(x,y,w,h);                        // only copy completed frame
                desktop_.ClearDamage();
            }
            DrawCursor();
            cursorMoved_=FALSE;
        }
        if(keyboard_)keyboard_->UpdateLEDs();
        // Main task yields to CNetTask, DHCP and CWebTask; also limits CPU usage.
        scheduler_.MsSleep(5);
    }
}
