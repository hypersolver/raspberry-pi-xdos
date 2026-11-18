#include "kernel.h"
#include <circle/bcmframebuffer.h>
#include <circle/device.h>
#include <circle/startup.h>
#include <assert.h>

CKernel *CKernel::instance_=0;

CKernel::CKernel()
: screen_(1024,768),timer_(&interrupt_),logger_(options_.GetLogLevel(),&timer_),
  usb_(&interrupt_,&timer_,TRUE),keyboard_(0),mouse_(0),
  keyHead_(0),keyTail_(0),mouseHead_(0),mouseTail_(0)
{
    instance_=this;
    actLED_.Blink(5);
}
CKernel::~CKernel() {instance_=0;}

boolean CKernel::Initialize()
{
    if(!screen_.Initialize())return FALSE;
    if(!serial_.Initialize(115200))return FALSE;
    // Keep diagnostics away from the graphics framebuffer.
    if(!logger_.Initialize(&serial_))return FALSE;
    if(!interrupt_.Initialize())return FALSE;
    if(!timer_.Initialize())return FALSE;
    if(!usb_.Initialize())return FALSE;
    CBcmFrameBuffer *fb=screen_.GetFrameBuffer();
    if(!fb || (fb->GetDepth()!=16 && fb->GetDepth()!=32))return FALSE;
    desktop_.Resize((int)fb->GetWidth(),(int)fb->GetHeight());
    desktop_.Render((void*)(unsigned long)fb->GetBuffer(),fb->GetPitch(),fb->GetDepth());
    return TRUE;
}

void CKernel::QueueKey(char ch)
{
    unsigned head=keyHead_;
    unsigned next=(head+1)%QUEUE;
    if(next==keyTail_)return;
    keys_[head]=ch;
    __sync_synchronize();
    keyHead_=next;
}
void CKernel::KeyHandler(const char *text)
{
    if(!instance_||!text)return;
    while(*text)instance_->QueueKey(*text++);
}
void CKernel::QueueMouse(unsigned event,unsigned buttons,unsigned x,unsigned y)
{
    unsigned head=mouseHead_;
    unsigned next=(head+1)%QUEUE;
    if(next==mouseTail_)return;
    mouseEvents_[head].event=event;
    mouseEvents_[head].buttons=buttons;
    mouseEvents_[head].x=x;
    mouseEvents_[head].y=y;
    __sync_synchronize();
    mouseHead_=next;
}
void CKernel::MouseHandler(TMouseEvent event,unsigned buttons,unsigned x,unsigned y,int wheel)
{
    (void)wheel;
    if(instance_)instance_->QueueMouse((unsigned)event,buttons,x,y);
}
void CKernel::KeyboardRemoved(CDevice *dev,void *ctx)
{
    (void)dev;(void)ctx;
    if(instance_)instance_->keyboard_=0;
}
void CKernel::MouseRemoved(CDevice *dev,void *ctx)
{
    (void)dev;(void)ctx;
    if(instance_)instance_->mouse_=0;
}
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
            if(m->Setup(fb)) {
                mouse_=m;
                m->RegisterRemovedHandler(MouseRemoved);
                m->SetCursor(screen_.GetWidth()/2,screen_.GetHeight()/2);
                m->ShowCursor(TRUE);
                m->RegisterEventHandler(MouseHandler);
            }
        }
    }
    desktop_.SetUSB(keyboard_!=0,mouse_!=0);
}
void CKernel::ProcessInput()
{
    while(keyTail_!=keyHead_) {
        unsigned tail=keyTail_;
        char ch=keys_[tail];
        __sync_synchronize();
        keyTail_=(tail+1)%QUEUE;
        desktop_.Key(ch);
    }
    while(mouseTail_!=mouseHead_) {
        unsigned tail=mouseTail_;
        MousePacket p;
        p.event=mouseEvents_[tail].event;
        p.buttons=mouseEvents_[tail].buttons;
        p.x=mouseEvents_[tail].x;
        p.y=mouseEvents_[tail].y;
        __sync_synchronize();
        mouseTail_=(tail+1)%QUEUE;
        Desktop::MouseAction action;
        switch(p.event) {
        case MouseEventMouseMove:action=Desktop::Move;break;
        case MouseEventMouseDown:action=Desktop::Down;break;
        case MouseEventMouseUp:action=Desktop::Up;break;
        default:continue;
        }
        desktop_.Mouse(action,p.buttons,(int)p.x,(int)p.y);
    }
}
void CKernel::Run()
{
    logger_.Write("xdos",LogNotice,"XD OS ARM32 V0.2: HDMI + combined USB HID");
    unsigned spin=0;
    while(1) {
        usb_.UpdatePlugAndPlay();
        AttachDevices();
        ProcessInput();
        if(desktop_.NeedsRedraw()) {
            // The Circle software cursor must be hidden around direct framebuffer writes.
            if(mouse_)mouse_->ShowCursor(FALSE);
            CBcmFrameBuffer *fb=screen_.GetFrameBuffer();
            desktop_.Render((void*)(unsigned long)fb->GetBuffer(),fb->GetPitch(),fb->GetDepth());
            if(mouse_)mouse_->ShowCursor(TRUE);
        }
        if(keyboard_)keyboard_->UpdateLEDs();
        if(mouse_)mouse_->UpdateCursor();
        if((++spin&0x3FF)==0)CTimer::SimpleMsDelay(1);
    }
}
