#ifndef XDOS_KERNEL_H
#define XDOS_KERNEL_H
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/input/mouse.h>
#include <circle/types.h>
#include "desktop.h"

class CKernel
{
public:
    CKernel();
    ~CKernel();
    boolean Initialize();
    void Run();
private:
    struct MousePacket {unsigned event,buttons,x,y;};
    static const unsigned QUEUE=64;
    static void KeyHandler(const char *text);
    static void MouseHandler(TMouseEvent event,unsigned buttons,unsigned x,unsigned y,int wheel);
    static void KeyboardRemoved(CDevice *dev,void *ctx);
    static void MouseRemoved(CDevice *dev,void *ctx);
    void AttachDevices();
    void ProcessInput();
    void QueueKey(char ch);
    void QueueMouse(unsigned event,unsigned buttons,unsigned x,unsigned y);
    static CKernel *instance_;

    // Order is important for Circle's hardware initialization.
    CActLED actLED_;
    CKernelOptions options_;
    CDeviceNameService names_;
    CScreenDevice screen_;
    CSerialDevice serial_;
    CExceptionHandler exception_;
    CInterruptSystem interrupt_;
    CTimer timer_;
    CLogger logger_;
    CUSBHCIDevice usb_;
    Desktop desktop_;
    CUSBKeyboardDevice *volatile keyboard_;
    CMouseDevice *volatile mouse_;
    volatile unsigned keyHead_,keyTail_,mouseHead_,mouseTail_;
    volatile char keys_[QUEUE];
    volatile MousePacket mouseEvents_[QUEUE];
};
#endif
