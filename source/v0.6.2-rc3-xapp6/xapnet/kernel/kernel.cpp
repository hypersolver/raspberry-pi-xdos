#include "xapp_loader.h"
#include "kernel.h"
#include "xdos_net_config.h"
#include "uart_service.h"
#include "benchmark_report.inc"

static XdosUartService &TerminalUart()
{
    static XdosUartService uart;
    return uart;
}
#include "xdos_text_keys.h"
#include "xapp_keypad.h"
#include <circle/actled.h>
#include <strings.h>
#include "xdos_arduino/Arduino.h"
#include "xdos_arduino/xdos_arduino_runtime.h"
#include "app_shortcut.h"
#include "xapp_v2_runtime.h"
#include "xdos_note_v2.h"
#include "xdos_files_v2.h"
#include "xdos_file_delete.h"
#include "xapp_loader_v2.h"
#include "xapp_loader.h"
#include "storage.h"
#include "p0_metrics.h"
#include "lvgl_vc4.h"
#include <circle/bcmpropertytags.h>
#include <circle/memory.h>
#include <vc4/vchiq/vchiqdevice.h>
extern "C" volatile int xdos_cpu_awake;
extern "C" volatile unsigned xdos_lvgl_due_us;
extern "C" volatile unsigned xdos_wfi_microseconds;
extern "C" volatile unsigned xdos_prof_irq_us, xdos_prof_irq_n, xdos_prof_fiq_us, xdos_prof_fiq_n, xdos_prof_idle_int_us;
// XDOS Tune: sleep in WFE until LVGL is due. The generic-timer event stream
// wakes WFE every ~0.43 ms, interrupts wake it too, so the deadline is met
// without the 10 ms tick of MsSleep. Time spent here counts as idle (CPU%).
static void XdosIdleUntilLvglDue()
{
    static bool eventStream = false;
    if (!eventStream) {
        unsigned ctl;
        asm volatile("mrc p15, 0, %0, c14, c1, 0" : "=r"(ctl));
        ctl = (ctl & ~0xF8U) | (12U << 4) | (1U << 2);   // EVNTI=12, EVNTEN
        asm volatile("mcr p15, 0, %0, c14, c1, 0" :: "r"(ctl));
        asm volatile("isb");
        eventStream = true;
    }
    const unsigned start = CTimer::GetClockTicks();
    int remaining = (int)(xdos_lvgl_due_us - start);
    if (remaining > 20000) remaining = 20000;
    if (remaining <= 300) return;
    const unsigned until = start + (unsigned)remaining - 200U;
    const unsigned i0 = xdos_prof_irq_us + xdos_prof_fiq_us;
    while ((int)(until - CTimer::GetClockTicks()) > 0) asm volatile("wfe");
    xdos_wfi_microseconds += CTimer::GetClockTicks() - start;
    xdos_prof_idle_int_us += xdos_prof_irq_us + xdos_prof_fiq_us - i0;
}
extern "C" unsigned xdos_vc4_clock_us() { return CTimer::GetClockTicks(); }
extern "C" void xdos_present_frame();
extern "C" void xdos_arduino_set_act_led(int on)
{
    if (on) CActLED::Get()->On();
    else CActLED::Get()->Off();
}
#include <circle/xdos_test_debug.h>
extern "C" { extern volatile unsigned xdos_wfi_entries; extern volatile unsigned xdos_wfi_microseconds; }
#include <circle/bcmframebuffer.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netconfig.h>
#include <circle/netdevice.h>
#include <circle/startup.h>
#include <circle/string.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include "player_host.inc"
#include "music_host.inc"
#ifndef XDOS_LVGL_PERF
#define XDOS_LVGL_PERF 0
#endif


// One-shot Circle cooperative task: postpone Ethernet PHY probing, Wi-Fi
// firmware loading, WPA2 and DHCP until the first desktop frame is visible.
// This task never draws the boot splash and does not block boot completion.
bool xdosBootIconsDrawn = false;
class CNetworkInitTask : public CTask
{
public:
    explicit CNetworkInitTask(CKernel *owner) : CTask(64*1024), owner_(owner) {}
    void Run(void) override
    {
        owner_->InitializeNetworkAfterDesktop();
    }
private:
    CKernel *owner_;
};

class CArduinoSketchTask : public CTask
{
public:
    CArduinoSketchTask() : CTask(32*1024) {}
    void Run(void) override
    {
        setup();
        for (;;) {
            loop();
            xdos_arduino_mark_loop();
            // A user sketch cannot be made fully preemptible with Circle's
            // cooperative scheduler; give other tasks at least one timeslice.
            CScheduler::Get()->MsSleep(1);
        }
    }
};

CKernel *CKernel::instance_=0;


CKernel::CKernel()
: screen_(0, 0), serial_(&interrupt_), timer_(&interrupt_), logger_(options_.GetLogLevel()<LogWarning ? options_.GetLogLevel() : LogWarning,&timer_),
  usb_(&interrupt_,&timer_,TRUE),
  emmc_(&interrupt_,&timer_,&actLED_),
#if XDOS_HAS_WIFI
  wlan_("SD:/firmware/"),
#endif
  net_(0),
#if XDOS_HAS_WIFI
  wpa_(0),
#endif
#if XDOS_USE_LVGL
  lvgl_(&screen_),
#endif
  storageTask_(0), storageMounted_(false),
  keyboard_(0), mouse_(0), keyHead_(0), keyTail_(0), mouseHead_(0), mouseTail_(0),
  debugSerialReady_(FALSE), networkInit_(FALSE), cursorVisible_(FALSE),
  lvglMousePresent_(FALSE),
  networkMode_(NetworkNone),
  back_(0),front_(0),pitch_(0),depth_(0),screenWidth_(0),screenHeight_(0),
  cursorX_(512),cursorY_(384),prevCursorX_(512),prevCursorY_(384),cursorMoved_(FALSE)
{
    instance_=this;
    actLED_.Blink(5);
}
CKernel::~CKernel() { instance_=0; }

void CKernel::Debug(const char *message)
{
    // Do not write with CScreenDevice::Write() during splash: it would corrupt
    // the pixels managed by CBootSplash. The same log is mirrored to UART.
    if(debugSerialReady_ && message) serial_.Write(message,strlen(message));
}

void CKernel::Boot(unsigned stage,const char *component,const char *detail,CBootSplash::TStatus status)
{
    boot_.Report(stage,component,detail,status);
    if ((status==CBootSplash::WARNING || status==CBootSplash::ERROR) &&
        boot_.GetLineCount()) {
        Debug(boot_.GetLine(boot_.GetLineCount()-1));
        Debug("\r\n");
    }
}

void CKernel::LogBootTiming(const char *stage)
{
#if 1
    CString line;
    line.Format("[BOOT PERF] %s=%u us\r\n",stage,
                (unsigned)(CTimer::GetClockTicks()-bootStartUs_));
    Debug((const char *)line);
#else
    (void)stage;
#endif
}

boolean CKernel::Initialize()
{
    bootStartUs_=CTimer::GetClockTicks();
    // Initialize IRQ before the interrupt-backed UART receive buffer.
    const boolean irqReady = interrupt_.Initialize();
    debugSerialReady_=irqReady && serial_.Initialize(921600);
    const auto usable = [](const CBcmFrameBuffer *fb) {
        return fb && fb->GetBuffer() && fb->GetDepth()==16 &&
               fb->GetWidth()>=640 && fb->GetWidth()<=4096 &&
               fb->GetHeight()>=480 && fb->GetHeight()<=2048 &&
               fb->GetPitch()>=fb->GetWidth()*2 && fb->GetPitch()<=16384 &&
               fb->GetSize()/fb->GetPitch()>=fb->GetHeight();
    };
    bool fallback = !screen_.Initialize() || !usable(screen_.GetFrameBuffer());
    if (fallback && (!screen_.Resize(1024,768) || !usable(screen_.GetFrameBuffer()))) {
        Debug("[BOOT ERROR] HDMI SCREEN INIT FAILED\r\n");
        return FALSE;
    }
    CBcmFrameBuffer *fb=screen_.GetFrameBuffer();
    CString resolution;
    resolution.Format("[DISPLAY] %ux%u RGB565 (%s)\r\n",fb->GetWidth(),fb->GetHeight(),
                      fallback ? "fallback request 1024x768" : "firmware auto");
    Debug(resolution);
    if(!boot_.Begin(fb)) {
        Debug("[BOOT ERROR] HDMI FRAMEBUFFER UNAVAILABLE\r\n");
        return FALSE;
    }
    Boot(1,"DISPLAY","HDMI Framebuffer ready");
    Boot(2,"UART",debugSerialReady_?"921600 8N1 enabled":"unavailable, using HDMI logs",
         debugSerialReady_?CBootSplash::OK:CBootSplash::WARNING);

    CDevice *logDevice=debugSerialReady_ ? (CDevice*)&serial_ : (CDevice*)&screen_;
    if(!logger_.Initialize(logDevice)) {
        Boot(3,"LOGGER","initialization failed",CBootSplash::ERROR);
        return FALSE;
    }
    Boot(3,"LOGGER","diagnostic messages active");
    if(!irqReady) {
        Boot(4,"IRQ","initialization failed",CBootSplash::ERROR);
        return FALSE;
    }
    Boot(4,"IRQ","ARM interrupts initialized");
    if(!timer_.Initialize()) {
        Boot(5,"TIMER","initialization failed",CBootSplash::ERROR);
        return FALSE;
    }
    Boot(5,"TIMER","system clock online");
    Boot(6,"USB HOST","starts after first desktop frame",CBootSplash::PENDING);

    if(!fb||!fb->GetBuffer()||!fb->GetPitch()) {
        Boot(7,"FRAMEBUFFER","invalid framebuffer",CBootSplash::ERROR);
        return FALSE;
    }
    if(fb->GetDepth()!=16 && fb->GetDepth()!=32) {
        Boot(7,"FRAMEBUFFER","unsupported color depth",CBootSplash::ERROR);
        return FALSE;
    }
    front_=(u8*)(unsigned long)fb->GetBuffer();
    pitch_=fb->GetPitch();
    depth_=fb->GetDepth();
    screenWidth_=fb->GetWidth();
    screenHeight_=fb->GetHeight();
    if(screenHeight_>2048 || pitch_>16384 || screenWidth_>4096) {
        Boot(7,"FRAMEBUFFER","dimensions exceed supported range",CBootSplash::ERROR);
        return FALSE;
    }
    const unsigned total=pitch_*screenHeight_;
#if !XDOS_USE_LVGL
    back_=new u8[total];
    if(!back_) {
        Boot(7,"MEMORY","allocating Back Buffer failed",CBootSplash::ERROR);
        return FALSE;
    }
    memset(back_,0,total);
    Boot(7,"VIDEO RAM","Back Buffer allocated; partial redraw enabled");
#else
    Boot(7,"VIDEO RAM","LVGL partial buffers; no full-screen XDOS back buffer");
#endif


    desktop_.Resize(screenWidth_,screenHeight_);
// V0.5.3: Do not initialize or probe Ethernet/WLAN/DHCP in the boot path.
// Input devices are discovered later by Run(), as before.
desktop_.SetNetwork(false,"NET: STARTS IN DESKTOP");
Boot(8,"NETWORK","Ethernet/Wi-Fi probing deferred",CBootSplash::PENDING);
#if XDOS_HAS_WIFI
Boot(9,"WIFI","SDIO firmware loading deferred",CBootSplash::PENDING);
#else
Boot(9,"WIFI","Not enabled for this board",CBootSplash::WARNING);
#endif
Boot(10,"DHCP","Starts after desktop is visible",CBootSplash::PENDING);
Boot(11,"BROWSER","HTTP worker starts with network",CBootSplash::PENDING);
Boot(12,"DESKTOP","Initializing desktop display",CBootSplash::PENDING);

    desktop_.Print("\n--- BOOT LOG ---\n");
    for(unsigned i=0;i<boot_.GetLineCount();++i) {
        desktop_.Print(boot_.GetLine(i));
        desktop_.Print("\n");
    }
    desktop_.Print("--- END BOOT LOG ---\n");

    // LVGL owns screen input and draw buffers when selected; never register
    // the legacy mouse event callback against the same CMouseDevice.
#if XDOS_USE_LVGL
    if (depth_ != 16) {
        Debug("[LVGL] requires 16-bit framebuffer; rebuild with DEPTH=16\r\n");
        return FALSE;
    }
    if (!lvgl_.Initialize()) {
        Debug("[LVGL] initialization failed\r\n");
        return FALSE;
    }
    boot_.Finish("BOOT READY - opening XD OS desktop");
    scheduler_.MsSleep(150);
#else
    boot_.Finish("BOOT READY - opening XD OS desktop");
    scheduler_.MsSleep(150);
    desktop_.Render(back_,pitch_,depth_);
    memcpy(front_,back_,total);
    desktop_.ClearDamage();
#endif
    return TRUE;
}


void CKernel::InitializeDesktopStorage()
{
    if(!storageMounted_) {
        LogBootTiming("icons_sd_start");
        if(emmc_.Initialize()) {
            LogBootTiming("icons_sd_ready");
            const FRESULT fr=f_mount(&filesystem_,"SD:",1);
            LogBootTiming("icons_mount_end");
            storageMounted_=(fr==FR_OK);
            desktop_.SetStorage(storageMounted_,storageMounted_?"FAT32: READY":"FAT32: MOUNT FAILED");
            if (storageMounted_) { static const char *const appNames[] = {
        "BROWSER", "CALC", "FILES", "LVGLBENCH", "LVGLDEMO", "MUSIC",
        "NOTE", "PAINT", "PLAYER", "TERMINAL"
    };
    unsigned installed = 0;
    for (unsigned i=0; i<sizeof appNames/sizeof *appNames; ++i) {
        char staged[]="SD:/XDOS/INBOX/MNAPP0.APP";
        staged[sizeof staged-6]=char('0'+i);
        char destination[48]="SD:/XDOS/APPS/";
        strcat(destination,appNames[i]);
        strcat(destination,".APP");
        if (f_rename(staged,destination)==FR_OK) ++installed;
    }
    if (f_rename("SD:/XDOS/INBOX/MNTERM.XAPP","SD:/XDOS/APPS/TERMINAL.XAPP")==FR_OK)
        Debug("[MCMEDIA] Installed Terminal executable\r\n");
    if (installed) Debug("[MCMEDIA] Installed staged APP manifests\r\n");
    LogBootTiming("icons_scan_start");
    static const char *const m4Files[] = {"QNCORE.XAPP","QNCORE.APP","QNNOTE.XAPP","QNNOTE.APP"};
    for (const char *m4 : m4Files) {   // XDOS MC4: replace on update
        CString from, to;
        from.Format("SD:/XDOS/INBOX/%s", m4);
        to.Format("SD:/XDOS/APPS/%s", m4);
        FILINFO staged{};
        if (f_stat((const char *)from, &staged)!=FR_OK) continue;
        f_unlink((const char *)to);
        if (f_rename((const char *)from, (const char *)to)==FR_OK) {
            CString line;
            line.Format("[MC4] Installed %s\r\n", m4);
            Debug((const char *)line);
        }
    }
    // Manifests of other MC4 builds point at modules bound to another kernel.
    f_rename("SD:/XDOS/APPS/M4CORE.APP","SD:/XDOS/APPS/M4CORE.OFF");
    f_rename("SD:/XDOS/APPS/M4NOTE.APP","SD:/XDOS/APPS/M4NOTE.OFF");
    f_rename("SD:/XDOS/APPS/M5CORE.APP","SD:/XDOS/APPS/M5CORE.OFF");
    f_rename("SD:/XDOS/APPS/M5NOTE.APP","SD:/XDOS/APPS/M5NOTE.OFF");
    f_rename("SD:/XDOS/APPS/M6CORE.APP","SD:/XDOS/APPS/M6CORE.OFF");
    f_rename("SD:/XDOS/APPS/M6NOTE.APP","SD:/XDOS/APPS/M6NOTE.OFF");
    f_rename("SD:/XDOS/APPS/M7CORE.APP","SD:/XDOS/APPS/M7CORE.OFF");
    f_rename("SD:/XDOS/APPS/M7NOTE.APP","SD:/XDOS/APPS/M7NOTE.OFF");
    f_rename("SD:/XDOS/APPS/M8CORE.APP","SD:/XDOS/APPS/M8CORE.OFF");
    f_rename("SD:/XDOS/APPS/M8NOTE.APP","SD:/XDOS/APPS/M8NOTE.OFF");
    f_rename("SD:/XDOS/APPS/M9CORE.APP","SD:/XDOS/APPS/M9CORE.OFF");
    f_rename("SD:/XDOS/APPS/M9NOTE.APP","SD:/XDOS/APPS/M9NOTE.OFF");
    f_rename("SD:/XDOS/APPS/MACORE.APP","SD:/XDOS/APPS/MACORE.OFF");
    f_rename("SD:/XDOS/APPS/MANOTE.APP","SD:/XDOS/APPS/MANOTE.OFF");
    f_rename("SD:/XDOS/INBOX/LEDBLINK.APP","SD:/XDOS/APPS/LEDBLINK.APP");
    f_rename("SD:/XDOS/INBOX/ARDLVGL.APP","SD:/XDOS/APPS/ARDLVGL.APP");
    desktop_.ScanDesktopApps();
    LogBootTiming("icons_scan_end");
    Debug("[MCMEDIA] Desktop catalog scanned\r\n"); LogBootTiming("fat32_ready"); }
            else Debug("[STORAGE ERROR] FAT32 mount failed\r\n");
            // Storage worker already started after desktop; SD and USB
            // commands share its serialized request queue.
        } else {
            desktop_.SetStorage(false,"FAT32: SD HOST FAILED");
            Debug("[STORAGE ERROR] SD host initialization failed\r\n");
        }
    }
}

void CKernel::InitializeNetworkAfterDesktop()
{
    // Runs as a separate Circle scheduler task, after desktop first paint.
    // Selection is still one-time: Ethernet link if present, else Wi-Fi.
    // FAT32 is needed by Notepad, XAPP and benchmark even with Ethernet link.
    scheduler_.MsSleep(350); // keep PHY probing deferred after drawing the shortcuts
    desktop_.SetNetwork(false,"ETH: DETECTING...");

    CNetDevice *wired=0;
    bool wiredLink=false;
    for(unsigned probe=0;probe<12;probe++) {
        usb_.UpdatePlugAndPlay();
        wired=CNetDevice::GetNetDevice(NetDeviceTypeEthernet);
        if(wired) {
            wired->UpdatePHY();
            if(wired->IsLinkUp()) {wiredLink=true;break;}
        }
        scheduler_.MsSleep(150);
    }

#if XDOS_HAS_WIFI
    bool wifiReady=false;
#endif
    if(wiredLink) {
        networkMode_=NetworkEthernet;
        desktop_.SetNetwork(false,"ETH: LINK UP / DHCP");
#if XDOS_HAS_WIFI
    } else {
        desktop_.SetNetwork(false,"WIFI: LOADING FW...");
        if(!storageMounted_) {
            Debug("[NET] SD/FAT32 not mounted; WLAN firmware unavailable\r\n");
        } else if(!wlan_.Initialize()) {
            Debug("[NET] WLAN firmware init failed\r\n");
        } else {
            wifiReady=true;
        }
        networkMode_=wifiReady ? NetworkWiFi : NetworkEthernet;
        if(!wifiReady) {
            desktop_.SetNetwork(false,"ETH: WAIT LINK");
        }
    }

#else
    } else {
        networkMode_=NetworkEthernet;
        desktop_.SetNetwork(false,"ETH: WAIT LINK (no onboard Wi-Fi)");
    }
#endif

    XdosStaticNetwork config{};
    const NetConfigResult configResult = storageMounted_
        ? ReadXdosNetworkConfig(config) : NetConfigMissing;
    const bool useStatic = configResult == NetConfigStatic;
    if (configResult == NetConfigInvalid)
        Debug("[NET] Invalid NET.CFG; using DHCP\r\n");
    Debug(useStatic ? "[NET] Address mode: static\r\n" : "[NET] Address mode: DHCP\r\n");
    net_=new CNetSubSystem(useStatic ? config.ip : nullptr,
             useStatic ? config.mask : nullptr, useStatic ? config.gateway : nullptr,
             useStatic ? config.dns : nullptr, "xdos-arm32",
             networkMode_==NetworkWiFi ? NetDeviceTypeWLAN : NetDeviceTypeEthernet);
    if(!net_) {
        desktop_.SetNetwork(false,"NET: ALLOC FAILED");
        Debug("[NET] Network allocation failed\r\n");
        return;
    }
    if(!net_->Initialize(FALSE)) { // MUST NOT wait indefinitely for DHCP
        desktop_.SetNetwork(false,"NET: INIT FAILED");
        Debug("[NET] TCP/IP init failed\r\n");
        return;
    }

#if XDOS_HAS_WIFI
    if(networkMode_==NetworkWiFi) {
        desktop_.SetNetwork(false,"WIFI: WPA2/DHCP");
        wpa_=new CWPASupplicant("SD:/wpa_supplicant.conf");
        if(!wpa_ || !wpa_->Initialize()) {
            desktop_.SetNetwork(false,"WIFI: WPA2 FAILED");
            Debug("[NET] WPA supplicant init failed\r\n");
            return;
        }
    } else
#endif
    {
        desktop_.SetNetwork(false,wiredLink?"ETH: WAIT DHCP":"ETH: WAIT LINK");
    }
    // Publish readiness only after the selected network's driver is initialized.
    networkInit_=TRUE;
    if (XDOS_TEST_DEBUG) Debug("[NET] Browser HTTP provided by XAPP on request\r\n");
}

void CKernel::QueueKey(unsigned key)
{
    unsigned h=keyHead_,next=(h+1)%QUEUE;
    if(next==keyTail_)return;
    keys_[h]=key;__sync_synchronize();keyHead_=next;
}
void CKernel::DesktopModifierHandler(unsigned char mods,const unsigned char keys[6])
{
    if (!instance_) return;
    // HID keyboard modifier bits 3 and 7 are left and right GUI.
    const bool pressed=(mods&0x88)!=0;
    if (pressed && !instance_->desktopWindowsHeld_) instance_->QueueKey(XdosKeyWindows);
    instance_->desktopWindowsHeld_=pressed;
    bool altF4=false;
    if (mods&0x44) {
        for (unsigned i=0;i<6;++i) if (keys[i]==0x3D) altF4=true;
    }
    if (altF4 && !instance_->desktopAltF4Held_) instance_->QueueKey(XdosKeyAltF4);
    instance_->desktopAltF4Held_=altF4;
}

void CKernel::KeyHandler(const char *text)
{
    if(!instance_||!text)return;
    if (*text == '\x1b' && text[1]) {
        static const struct { const char *sequence; XdosTextKey key; } keys[] = {
            {"\x1b[D",XdosKeyLeft}, {"\x1b[C",XdosKeyRight},
            {"\x1b[A",XdosKeyUp}, {"\x1b[B",XdosKeyDown},
            {"\x1b[1~",XdosKeyHome}, {"\x1b[4~",XdosKeyEnd},
            {"\x1b[3~",XdosKeyDelete}, {"\x1b[2~",XdosKeyInsert},
        };
        for (const auto &key : keys) {
            if (!strcmp(text,key.sequence)) {
                instance_->QueueKey(key.key);
                break;
            }
        }
        return;
    }
    while (*text) instance_->QueueKey(static_cast<unsigned char>(*text++));
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

void CKernel::AttachDevices(boolean plugUpdated, unsigned nowUs)
{
    // Hotplug gets immediate attention. With no change, scan the device-name
    // registry only 4 times/second instead of every 5 ms GUI iteration.
    static unsigned lastScanUs = 0;
    if (!plugUpdated && lastScanUs != 0 &&
        (unsigned)(nowUs - lastScanUs) < 250000U) return;
    lastScanUs = nowUs;
    if(!keyboard_) {
        CUSBKeyboardDevice *kbd=(CUSBKeyboardDevice*)names_.GetDevice("ukbd1",FALSE);
        if(kbd) {
            keyboard_=kbd;
            kbd->RegisterRemovedHandler(KeyboardRemoved);
            desktopWindowsHeld_=false;
            desktopAltF4Held_=false;
            kbd->RegisterKeyStatusHandlerRaw(DesktopModifierHandler,TRUE);
            kbd->RegisterKeyPressedHandler(KeyHandler);
        }
    }
#if !XDOS_USE_LVGL
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
#endif
    desktop_.SetUSB(keyboard_!=0,
#if XDOS_USE_LVGL
        (lvglMousePresent_ = names_.GetDevice("mouse1",FALSE)!=0)
#else
        mouse_!=0
#endif
    );
}

void CKernel::ProcessInput()
{
    while(keyTail_!=keyHead_) {
        unsigned t=keyTail_;const unsigned key=keys_[t];
        __sync_synchronize();keyTail_=(t+1)%QUEUE;
#if XDOS_USE_LVGL
        if (XappV2Runtime::DeliverKey(key)) continue;   // XDOS MC4
        if (ui_.HandleTaskKey(key) || ui_.OnKey(key)) continue;
        if (desktop_.DesktopNavigationKey(key)) continue;
#endif
        if (key < 256) desktop_.Key(static_cast<char>(key));
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
        // XDOS MC4: a press on a V2 window (and its drag/release) is not a desktop click.
        if (XappV2Runtime::Pointer(action==Desktop::Down ? 1 : action==Desktop::Up ? 2 : 0,
                                   (int)p.x,(int)p.y)) continue;
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

namespace {
class BrowserXappJob : public CTask {
public:
    BrowserXappJob(XdosBrowserFetch fetch,CNetSubSystem *network,Desktop *desktop,
                   const char *url,unsigned id)
    : fetch_(fetch),network_(network),desktop_(desktop),id_(id)
    {
        strcpy(url_,url);
        SetName("browser-xapp");
    }

    void Run() override
    {
        if (XDOS_TEST_DEBUG) CLogger::Get()->Write("BROWSER",LogWarning,"XAPP HTTP request started");
        fetch_(network_,desktop_,url_,id_);
        XappLoaderV1::ReleaseSlot();
        if (XDOS_TEST_DEBUG) CLogger::Get()->Write("BROWSER",LogWarning,"XAPP HTTP request finished");
    }

private:
    XdosBrowserFetch fetch_;
    CNetSubSystem *network_;
    Desktop *desktop_;
    unsigned id_;
    char url_[192];
};
}

bool CKernel::StartBrowserRequest(const char *url)
{
    if (!networkInit_ || !net_ || !net_->IsRunning()) {
        desktop_.BrowserError("NO NETWORK: Wait for Wi-Fi/Ethernet DHCP.");
        return false;
    }
    if (!browserFetch_ || !XappLoaderV1::HoldSlot()) return false;
    char pending[192];
    unsigned id;
    if (desktop_.RequestHttp(url) && desktop_.TakeBrowserRequest(pending,sizeof pending,id)) {
        if (new BrowserXappJob(browserFetch_,net_,&desktop_,pending,id)) return true;
        desktop_.BrowserError("Could not start Browser XAPP job.");
    }
    XappLoaderV1::ReleaseSlot();
    return false;
}

void CKernel::UpdateNetworkStatus()
{
    if(!networkInit_ || !net_)return;
    if(!net_->IsRunning()) {
        desktop_.SetNetwork(false,networkMode_==NetworkWiFi ?
            "WIFI: WAIT WPA/DHCP":"ETH: WAIT DHCP/LINK");
        return;
    }
    const u8 *ip=net_->GetConfig()->GetIPAddress()->Get();
    if(!ip)return;
    if (!bootNetworkLogged_ && (ip[0] || ip[1] || ip[2] || ip[3])) {
        LogBootTiming("network_ready");
        bootNetworkLogged_=true;
    }
    CString state;
    state.Format("%s %u.%u.%u.%u",networkMode_==NetworkWiFi?"WIFI":"ETH",
        (unsigned)ip[0],(unsigned)ip[1],(unsigned)ip[2],(unsigned)ip[3]);
    desktop_.SetNetwork(true,(const char*)state);
}

void CKernel::Run()
{
    Debug("[XDOS] V0.6.2-RC3 XAPP6-PERF7-VC4-AnalyticsRegionGuard-FILESV2 / Pi 3 ARM32\r\n");
#if XDOS_USE_LVGL
    ui_.Initialize(&desktop_,debugSerialReady_ ? &serial_ : 0);
    static CVCHIQDevice gpu(CMemorySystem::Get(),&interrupt_);
    musicInterrupt=&interrupt_;
    const bool gpuReady=gpu.Initialize();
    if (gpuReady) musicGpu=&gpu;
    if (gpuReady && xdos_vc4_start(screenWidth_,screenHeight_)) {
        if (XDOS_TEST_DEBUG) Debug("[VC4 LVGL] GLES2 presentation active; EGL swap interval=1\r\n");
    } else {
        Debug("[VC4 LVGL] VSyncOpt fallback\r\n");
    }
    auto *lvDisplay=lv_display_get_default();
    unsigned vc4Width,vc4Height,hdmiWidth,hdmiHeight;
    xdos_vc4_get_geometry(&vc4Width,&vc4Height,&hdmiWidth,&hdmiHeight);
    CString geometry;
    geometry.Format("[DISPLAY] XDOS=%ux%u LVGL=%ux%u VC4=%ux%u HDMI=%ux%u\r\n",
                    (unsigned)desktop_.Width(),(unsigned)desktop_.Height(),
                    (unsigned)lv_display_get_horizontal_resolution(lvDisplay),
                    (unsigned)lv_display_get_vertical_resolution(lvDisplay),
                    vc4Width,vc4Height,hdmiWidth,hdmiHeight);
    Debug(geometry);
    lv_refr_now(lv_display_get_default());
    const unsigned flushStartUs=CTimer::GetClockTicks();
    while (xdos_p0_dma_submitted != xdos_p0_dma_completed &&
           (unsigned)(CTimer::GetClockTicks()-flushStartUs) < 100000U) {
        scheduler_.MsSleep(1);
    }
    xdos_present_frame();
    LogBootTiming(xdos_p0_dma_submitted == xdos_p0_dma_completed ?
                  "desktop_first_flush" : "desktop_flush_timeout");
#endif
    InitializeDesktopStorage();
#if XDOS_USE_LVGL
    if (storageMounted_) {
        ui_.Refresh(CTimer::GetClockTicks());
        lv_refr_now(lv_display_get_default());
        const unsigned iconsFlushStart=CTimer::GetClockTicks();
        while (xdos_p0_dma_submitted != xdos_p0_dma_completed &&
               (unsigned)(CTimer::GetClockTicks()-iconsFlushStart) < 100000U)
            scheduler_.MsSleep(1);
        xdos_present_frame();
        if (xdosBootIconsDrawn) LogBootTiming("icons_first_draw");
    }
#endif
    LogBootTiming("usb_start");
    if (!usb_.Initialize()) {
        Debug("[BOOT ERROR] USB HOST initialization failed\r\n");
        boot_.Report(6,"USB HOST","initialization failed",CBootSplash::ERROR);
        return;
    }
    LogBootTiming("usb_ready");
    AttachDevices(TRUE,CTimer::GetClockTicks());
#if XDOS_USE_LVGL
    lvgl_.Update(TRUE);
    if (lvglMousePresent_) LogBootTiming("mouse_detected");
#endif
    if (keyboard_) LogBootTiming("keyboard_detected");
    if(!storageTask_)storageTask_=new CStorageTask(&desktop_);
    new CNetworkInitTask(this);
    xdos_arduino_attach_serial(debugSerialReady_ ? &serial_ : 0);
    new CArduinoSketchTask();
    // LCD status updates are event-driven; poll at a bounded UI cadence.
    // This avoids loop-speed-dependent repaint rates on Pi1/2/3/4.
    unsigned lastUiPollUs = 0;
    unsigned lastNetworkPollUs = 0;
    unsigned lastLedPollUs = 0;
    bool iconsReported = xdosBootIconsDrawn;
#if XDOS_USE_LVGL && XDOS_LVGL_PERF
    unsigned perfStartUs = CTimer::GetClockTicks();
    unsigned perfCalls = 0, perfMaxUs = 0;
    u64 perfTotalUs = 0;
    unsigned perfLabels = ui_.TextUpdateCount();
    unsigned perfDmaFlushes = xdos_p0_dma_completed;
    unsigned perfCpuFlushes = xdos_p0_cpu_flushes;
    unsigned idleBaseUs = xdos_wfi_microseconds;
    unsigned idleBaseTicks = perfStartUs;
#endif
    static XdosUartService &uart = TerminalUart();
    Debug("[XDOS UART] READY 1\r\n");
    while(1) {
        if (debugSerialReady_) {
            uart.SetNetwork(networkInit_ ? net_ : nullptr);
            uart.Poll(serial_,storageMounted_);
        }
        boolean plugUpdated=usb_.UpdatePlugAndPlay();
        const unsigned nowUs = CTimer::GetClockTicks();
        AttachDevices(plugUpdated, nowUs);
        ProcessInput();
        using Shortcut = XdosUartService::Shortcut;
        const auto shortcut=uart.TakeOpenRequest();
        const bool uartOpen=shortcut!=Shortcut::None;
#if XDOS_USE_LVGL
        bool v2ShortcutOpened=false;
#endif
#if XDOS_USE_LVGL
        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();
        const char *fileModule = canOpen && !uartOpen ? XdosFilesTakeLaunch() : nullptr;
        if (fileModule && desktop_.OpenCatalogModule(fileModule)) fileModule=nullptr;
        const auto power=desktop_.TakePowerRequest();
        if (power!=Desktop::PowerRequest::None && ui_.AtDesktop()) {
            if (uart.PowerTransferActive() || desktop_.StorageWorkPending()) {
                desktop_.ReportAppResult("Power action unavailable while storage is busy");
            } else {
                const bool restart=power==Desktop::PowerRequest::Restart;
                if (storageMounted_) {
                    f_mount(nullptr,"SD:",0);
                    storageMounted_=false;
                }
                if (restart) desktop_.ReportAppResult("Restarting XDOS...");
                else desktop_.ShowShutdownNotice();
                ui_.Refresh(CTimer::GetClockTicks());
                lv_refr_now(lv_display_get_default());
                scheduler_.MsSleep(40);
                xdos_present_frame();
                Debug(restart ? "[POWER] Desktop restart\r\n" : "[POWER] Desktop shutdown\r\n");
                if (restart) reboot();
                scheduler_.MsSleep(2000);
                // The cursor lives on the system layer, above the desktop canvas.
                auto *black=lv_obj_create(lv_layer_sys());
                lv_obj_remove_style_all(black);
                lv_obj_set_size(black,lv_pct(100),lv_pct(100));
                lv_obj_set_pos(black,0,0);
                lv_obj_set_style_bg_color(black,lv_color_black(),0);
                lv_obj_set_style_bg_opa(black,LV_OPA_COVER,0);
                lv_obj_move_foreground(black);
                lv_refr_now(lv_display_get_default());
                xdos_present_frame();
                bool hdmiOff=xdos_vc4_power_off_hdmi();
                if (!hdmiOff) {
                    // Firmware framebuffer blank request (tag 0x00040002).
                    struct { TPropertyTag Tag; u32 state; } blank = {{},1};
                    hdmiOff=CBcmPropertyTags().GetTag(0x00040002,&blank,sizeof blank,sizeof blank.state);
                }
                Debug(hdmiOff ? "[POWER] Display off requested\r\n" :
                                "[POWER] Display off failed\r\n");
                scheduler_.MsSleep(150);
                halt();
            }
        }
#endif
        XdosHostApiV1 api={XDOS_ABI_VERSION,sizeof(XdosHostApiV1),this,
                           SDKLog,SDKShowDemo,SDKShowBenchmark};
        static XdosNoteV2Api v2api;
        v2api = {
            api, XDOS_NOTE_V2_CAPABILITY,
            [](void *ctx) -> int {
                return static_cast<CKernel *>(ctx)->desktop_.LoadNote() ? 0 : -1;
            },
            [](void *ctx, const char *text) -> int {
                return text && strlen(text) <= XDOS_NOTE_MAX_BYTES
                    && static_cast<CKernel *>(ctx)->desktop_.SaveNoteText(text) ? 0 : -1;
            },
            [](void *ctx, XdosNoteSnapshot *out) {
                const auto &desktop = static_cast<CKernel *>(ctx)->desktop_;
                *out = {desktop.NoteStorageRevision(), desktop.NoteRevision(),
                        desktop.NoteText(), desktop.NoteStatus()};
            }
        };
        v2api.base.struct_bytes = sizeof v2api;
        static XdosFilesV2Api filesApi;
        filesApi={api,XDOS_FILES_V2_CAPABILITY,xdos_directory_visit,XdosFilesPreview,
                  [](const char *path) -> int { return XdosDeleteFile(path); },
                  XdosFilesQueueLaunch,XdosFilesClaim,XdosFilesSetPanes};
        filesApi.base.struct_bytes=sizeof filesApi;
        const char *const filesPath="SD:/XDOS/INBOX/FVFILES.XAPP";
        auto v2Host=[&](const char *path) {
            return !strcasecmp(path,filesPath) ? &filesApi.base : &v2api.base;
        };

        {   // XDOS_CORE_JOBS: V2 app runtime (core 0)
            XappV2Runtime::Service();
            if (debugSerialReady_) {
                char name[40];
                if (uart.TakeXapp2Open(name, sizeof name)) {
                    CString path;
                    path.Format("SD:/XDOS/INBOX/%s", name);
                    const int slot = storageMounted_ ?
                        XappV2Runtime::Open((const char *)path, v2Host(path)) : -1;
                    CString reply;
                    if (slot >= 0) reply.Format("@XDOS XAPP2 SLOT %d @XDOS OK\r\n", slot);
                    else reply.Format("@XDOS XAPP2 FAILED %d %s @XDOS ERROR\r\n", slot,
                                      XappLoaderV2::LastError());
                    XdosUartService::Send(serial_, (const char *)reply);
                }
                const int closeSlot = uart.TakeXapp2Close();
                if (closeSlot >= 0)
                    XdosUartService::Send(serial_, XappV2Runtime::Close(closeSlot) ?
                        "@XDOS XAPP2-CLOSE @XDOS OK\r\n" : "@XDOS ERROR\r\n");
                if (uart.TakeXapp2Status()) {
                    char status[160];
                    XappV2Runtime::Status(status, sizeof status);
                    CString reply;
                    reply.Format("@XDOS XAPP2-STATUS %s @XDOS OK\r\n", status);
                    XdosUartService::Send(serial_, (const char *)reply);
                }
            }
        }
#if XDOS_USE_LVGL
        if ((uartOpen ? shortcut==Shortcut::Calculator : desktop_.TakeCalculatorRequest()) &&
            ui_.AtDesktop() && (!uartOpen || canOpen)) {
            XdosKeypadApi keypad = {api, [](void *ctx, const char *title, const char *keys,
                                         XdosKeypadInput input) -> int32_t {
                return static_cast<CKernel *>(ctx)->ui_.OpenKeypad(title, keys, input);
            }};
            keypad.base.struct_bytes = sizeof keypad;
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunCalculatorShortcut(&keypad.base) : "XAPP: FAT32 NOT READY");
        }
#endif
#if XDOS_USE_LVGL
        if ((uartOpen ? shortcut==Shortcut::Notepad : desktop_.TakeNotepadRequest()) &&
            ui_.AtDesktop() && (!uartOpen || canOpen)) {
            const int slot = storageMounted_ ?
                XappV2Runtime::Open("SD:/XDOS/INBOX/FVNOTE.XAPP", &v2api.base) : -1;
            v2ShortcutOpened=slot>=0;
            desktop_.ReportAppResult(slot >= 0 ? "Notepad started" : "Notepad could not start");
        }
#endif
#if XDOS_USE_LVGL
        auto workspace = desktop_.TakeWorkspaceRequest();
        if (uartOpen) {
            switch (shortcut) {
            case Shortcut::Paint: workspace=Desktop::PaintApp; break;
            case Shortcut::Files: workspace=Desktop::FilesApp; break;
            case Shortcut::Browser: workspace=Desktop::BrowserApp; break;
            case Shortcut::Terminal: workspace=Desktop::TerminalApp; break;
            case Shortcut::Player: case Shortcut::Play: workspace=Desktop::PlayerApp; break;
            case Shortcut::Music: workspace=Desktop::MusicApp; break;
            default: workspace=Desktop::NoWorkspace; break;
            }
        }
        if (workspace==Desktop::FilesApp && canOpen) {
            const int slot=storageMounted_ ? XappV2Runtime::Open(filesPath,&filesApi.base) : -1;
            v2ShortcutOpened=slot>=0;
            if (slot>=0) Debug("[FILES] External V2 window opened\r\n");
            desktop_.ReportAppResult(slot>=0 ? "Files started" : "Files could not start");
            workspace=Desktop::NoWorkspace;
        }
        if (workspace != Desktop::NoWorkspace && ui_.AtDesktop() && (!uartOpen || canOpen)) {
            XdosWorkspaceApi apiExt = {{{api, nullptr}, nullptr},
                [](void *ctx, const char *title, XdosStroke stroke) -> int32_t {
                    return static_cast<CKernel *>(ctx)->ui_.OpenCanvas(title,stroke);
                },
                [](void *ctx, int panel, const char *title, const char *hint, const char *buttons,
                        XdosWorkspaceAction action) -> int32_t {
                    return static_cast<CKernel *>(ctx)->ui_.OpenWorkspace(panel,title,hint,buttons,action,
                        [](void *opaque, int op, const char *text) -> int32_t {
                            auto *k = static_cast<CKernel *>(opaque);
                            switch (op) {
                            case XDOS_REQUEST_TERMINAL: {
                                if (!text || !*text || strlen(text)>=160) return -1;
                                const XdosCommandReply reply{&k->desktop_,[](void *context,const char *message) {
                                    static_cast<Desktop *>(context)->Print(message);
                                }};
                                char command[160];
                                const char *p=text;
                                while (*p==' ' || *p=='\t') ++p;
                                unsigned used=0;
                                for (; *p; ++p) {
                                    const char ch=*p;
                                    if (ch=='\r' || ch=='\n') return -1;
                                    command[used++]=ch>='a' && ch<='z' ? ch-'a'+'A' : ch;
                                }
                                while (used && command[used-1]==' ') --used;
                                command[used]=0;
                                if (!TerminalUart().RunTerminalCommand(command,reply)) {
                                    k->desktop_.RunLegacyTerminal(text);
                                }
                                return 0;
                            }
                            case XDOS_REQUEST_LIST: return k->desktop_.RequestList() ? 0 : -1;
                            case XDOS_REQUEST_LIST_USB: return k->desktop_.ListUSB() ? 0 : -1;
                            case XDOS_REQUEST_READ: return k->desktop_.RequestRead(text) ? 0 : -1;
                            case XDOS_REQUEST_MUSIC_PLAY: return StartMusic(text,VCHIQSoundDestinationHDMI) ? 0 : -1;
                            case XDOS_REQUEST_MUSIC_JACK: return StartMusic(text,VCHIQSoundDestinationHeadphones) ? 0 : -1;
                            case XDOS_REQUEST_MUSIC_STOP: StopMusic(); return 0;
                            case XDOS_REQUEST_PLAY: return QueueVideo(text) ? 0 : -1;
                            case XDOS_REQUEST_HTTP: return k->StartBrowserRequest(text) ? 0 : -1;
                            case XDOS_REQUEST_CANCEL_HTTP: k->desktop_.CancelBrowser(); return 0;
                            default: return -1;
                            }
                        },ctx);
                },
                [](void *ctx,XdosBrowserFetch fetch) -> int32_t {
                    if (!fetch) return -1;
                    static_cast<CKernel *>(ctx)->browserFetch_=fetch;
                    return 0;
                }};
            if (workspace==Desktop::BrowserApp) browserFetch_=nullptr;
            apiExt.editor.keypad.base.struct_bytes = sizeof apiExt;
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunWorkspaceShortcut(workspace,&apiExt.editor.keypad.base) : "XAPP: FAT32 NOT READY");
        }
#endif
        const bool openWidgets = desktop_.TakeAppLaunchRequest();
#if XDOS_USE_LVGL
        XdosArduinoLvglApi sketch = {api, [](void *ctx, XdosSketchRoutine setup,
                                             XdosSketchRoutine loop, XdosSketchRoutine stop,
                                             int measured) -> int32_t {
            return static_cast<CKernel *>(ctx)->ui_.RunArduinoSketch(setup,loop,stop,measured);
        }};
        sketch.base.struct_bytes = sizeof sketch;
        if (fileModule) {
            const char *leaf=fileModule;
            for (const char *p=fileModule;*p;++p) if (*p=='/') leaf=p+1;
            FIL file;
            uint32_t header[2]={};
            UINT bytes=0;
            if (f_open(&file,fileModule,FA_READ)==FR_OK) {
                f_read(&file,header,sizeof header,&bytes);
                f_close(&file);
            }
            const bool v2=bytes==sizeof header && !memcmp(header,"XDAP",4)
                && header[1]==XDOS_XAPP_V2_ABI;
            const bool started=v2 ? XappV2Runtime::Open(fileModule,v2Host(fileModule))>=0
                : XappLoaderV1::ExecuteFile(fileModule,&sketch.base)==XappLoaderV1::OK;
            if (started && !v2 && !strcasecmp(leaf,"QNIMAGE.XAPP"))
                ui_.SetAppTaskTitle("Arduino IMAGE VIEW");
            desktop_.ReportAppResult(started ?
                "XAPP: application started" : "XAPP: invalid or incompatible application");
        }
        if (const auto *app=desktop_.TakeDiscoveredApp()) {
            const bool notepad = !strcasecmp(app->path, "SD:/XDOS/APPS/NOTE.XAPP");
            const bool files=!strcasecmp(app->path,"SD:/XDOS/APPS/FILES.XAPP");
            if (files || notepad || app->kind==XdosAppKind::CoreApp) {
                // XDOS MC4: V2 app -> lowest free slot; it borrows a free core.
                const char *path=files ? filesPath : notepad ? "SD:/XDOS/INBOX/FVNOTE.XAPP" : app->path;
                const int slot = !uartOpen && canOpen && storageMounted_ ?
                    XappV2Runtime::Open(path, v2Host(path)) : -100;
                CString line;
                if (slot >= 0) line.Format("XAPP: %s started (slot %d)", app->name, slot);
                else if (slot == -100) line = "XAPP: desktop busy";
                else line.Format("XAPP: %s not started (%s)", app->name, XappLoaderV2::LastError());
                desktop_.ReportAppResult((const char *)line);
                CString log;
                log.Format("[MC4] desktop open %s -> %d\r\n", app->path, slot);
                Debug((const char *)log);
            }
            else if (!uartOpen && canOpen && storageMounted_) {
                const bool arduino=app->kind==XdosAppKind::Arduino;
                const auto result=XappLoaderV1::ExecuteFile(
                    app->path, arduino ? &sketch.base : &api, arduino);
                if (arduino && result==XappLoaderV1::OK) ui_.SetAppTaskTitle(app->name);
                desktop_.ReportAppResult(result==XappLoaderV1::OK ?
                    "XAPP: application started" : "XAPP: invalid or incompatible application");
            }
        }
        const bool openBuiltinWidgets = desktop_.TakeLVGLRequest();
        if ((uartOpen ? shortcut==Shortcut::Widgets : openWidgets || openBuiltinWidgets) &&
            ui_.AtDesktop() && (!uartOpen || canOpen)) {
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunLVGLShortcut(&sketch.base) : "XAPP: FAT32 NOT READY");
        }
        if ((uartOpen ? shortcut==Shortcut::Benchmark : desktop_.TakeOfficialBenchmarkLaunchRequest()) &&
            ui_.AtDesktop() && (!uartOpen || canOpen)) {
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunBenchmarkShortcut(&sketch.base) : "XAPP: FAT32 NOT READY");
        }
#else
        if (!uartOpen && openWidgets) {
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunLVGLShortcut(&api) : "XAPP: FAT32 NOT READY");
        }
#endif
#if XDOS_USE_LVGL

        if (shortcut==Shortcut::Play) {
            const bool accepted=canOpen && !ui_.AtDesktop() && QueueVideo(uart.PlayPath());
            if (!accepted && canOpen && !ui_.AtDesktop()) ui_.CloseCurrentApp();
            serial_.Write(accepted ? "@XDOS PLAYING\r\n" : "@XDOS ERROR\r\n",
                          accepted ? 15 : 13);
        }
        else if (shortcut==Shortcut::Close) uart.CompleteClose(serial_,ui_.CloseCurrentApp());
        else if (uartOpen) uart.CompleteOpen(serial_,v2ShortcutOpened || (canOpen && !ui_.AtDesktop()));
        PollMusic();
        if (playerPath[0]) {
            const int result=RunQueuedVideo(debugSerialReady_ ? &serial_ : nullptr);
            desktop_.ReportAppResult(result ? "MP4 playback failed or unsupported" : "MP4 playback ended");
            if (debugSerialReady_) {
                const char *message=result ? "@XDOS PLAY ERROR\r\n" : "@XDOS PLAY DONE\r\n";
                serial_.Write(message,strlen(message));
            }
            desktop_.RequestFullRepaint();
        }
        ui_.Tick();
#else
        if (uartOpen) uart.CompleteOpen(serial_,false);
#endif
#if XDOS_USE_LVGL
        if ((unsigned)(nowUs - lastUiPollUs) >= 33000U || lastUiPollUs == 0) {
            ui_.Refresh(nowUs);  // update model BEFORE LVGL flush
            lastUiPollUs = nowUs;
        }
#if XDOS_LVGL_PERF
        const unsigned lvglStartUs = CTimer::GetClockTicks();
        const unsigned lvglInt0 = xdos_prof_irq_us + xdos_prof_fiq_us;
#endif
        lvgl_.Update(plugUpdated);
        if (!iconsReported && xdosBootIconsDrawn) {
            LogBootTiming("icons_first_draw");
            iconsReported = true;
        }
#if XDOS_LVGL_PERF
        const unsigned lvglCostUs = (unsigned)(CTimer::GetClockTicks() - lvglStartUs);
        perfTotalUs += lvglCostUs;
        {   // XDOS CPU profile: LVGL time without the interrupts that hit it
            static u64 profLvglUs;
            static unsigned profIrq0, profIrqN0, profFiq0, profFiqN0, profIdleInt0;
            const unsigned lvglInt = xdos_prof_irq_us + xdos_prof_fiq_us - lvglInt0;
            profLvglUs += lvglCostUs > lvglInt ? lvglCostUs - lvglInt : 0U;
            if ((unsigned)(nowUs - perfStartUs) >= 5000000U) {
                const unsigned win = nowUs - idleBaseTicks;
                const unsigned idleUs = xdos_wfi_microseconds - idleBaseUs;
                const unsigned irqUs = xdos_prof_irq_us - profIrq0, fiqUs = xdos_prof_fiq_us - profFiq0;
                const unsigned idleInt = xdos_prof_idle_int_us - profIdleInt0;
                const unsigned busyUs = win > idleUs ? win - idleUs : 0U;
                const unsigned intUs = irqUs + fiqUs > idleInt ? irqUs + fiqUs - idleInt : 0U;
                const unsigned lvUs = (unsigned)profLvglUs;
                const unsigned otherUs = busyUs > intUs + lvUs ? busyUs - intUs - lvUs : 0U;
                auto pm = [win](unsigned us) { return win ? (unsigned)((u64)us * 1000U / win) : 0U; };
                CString line;
                line.Format("[PROF] win=%u busy=%u.%u int=%u.%u (irq %u.%u %u/s, fiq %u.%u %u/s) "
                            "lvgl=%u.%u other=%u.%u idle_int=%u.%u\r\n", win / 1000U,
                            pm(busyUs) / 10, pm(busyUs) % 10, pm(intUs) / 10, pm(intUs) % 10,
                            pm(irqUs) / 10, pm(irqUs) % 10,
                            win ? (unsigned)((u64)(xdos_prof_irq_n - profIrqN0) * 1000000U / win) : 0U,
                            pm(fiqUs) / 10, pm(fiqUs) % 10,
                            win ? (unsigned)((u64)(xdos_prof_fiq_n - profFiqN0) * 1000000U / win) : 0U,
                            pm(lvUs) / 10, pm(lvUs) % 10, pm(otherUs) / 10, pm(otherUs) % 10,
                            pm(idleInt) / 10, pm(idleInt) % 10);
                serial_.Write((const char *)line, line.GetLength());
                profLvglUs = 0;
                profIrq0 = xdos_prof_irq_us; profIrqN0 = xdos_prof_irq_n;
                profFiq0 = xdos_prof_fiq_us; profFiqN0 = xdos_prof_fiq_n;
                profIdleInt0 = xdos_prof_idle_int_us;
            }
        }
        if (lvglCostUs > perfMaxUs) perfMaxUs = lvglCostUs;
        ++perfCalls;
        if ((unsigned)(nowUs - perfStartUs) >= 5000000U) {
            const unsigned labels = ui_.TextUpdateCount();
            const unsigned elapsedUs = nowUs-idleBaseTicks;
            const unsigned idleUs = xdos_wfi_microseconds-idleBaseUs;
            const unsigned idlePct = elapsedUs ?
                (unsigned)(((u64)idleUs * 100U / elapsedUs) > 100U ? 100U :
                           ((u64)idleUs * 100U / elapsedUs)) : 0U;
            const unsigned dmaFlushes = xdos_p0_dma_completed;
            const unsigned cpuFlushes = xdos_p0_cpu_flushes;
            const unsigned cpuPercent = 100U-idlePct;
            uart.SetLvglStats({elapsedUs/1000U,
                               perfCalls ? (unsigned)(perfTotalUs / perfCalls) : 0U,
                               perfMaxUs,perfCalls,labels-perfLabels,cpuPercent,
                               dmaFlushes-perfDmaFlushes,cpuFlushes-perfCpuFlushes,idleUs});
            ui_.UpdateMonitor(cpuPercent);
            perfDmaFlushes=dmaFlushes;
            perfCpuFlushes=cpuFlushes;
            idleBaseUs=xdos_wfi_microseconds;
            idleBaseTicks=nowUs;
            perfStartUs = nowUs;
            perfCalls = perfMaxUs = 0;
            perfTotalUs = 0;
            perfLabels = labels;
        }
#endif
#endif
        if ((unsigned)(nowUs - lastNetworkPollUs) >= 250000U || lastNetworkPollUs == 0) {
            UpdateNetworkStatus();
            lastNetworkPollUs = nowUs;
        }
#if !XDOS_USE_LVGL
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
#endif
        if (keyboard_ && ((unsigned)(nowUs - lastLedPollUs) >= 100000U ||
                          lastLedPollUs == 0)) {
            keyboard_->UpdateLEDs();
            lastLedPollUs = nowUs;
        }
        // Main task yields to deferred network task, CNetTask and Browser worker.
        // XDOS Tune: no WFI while an XAPP window is open.
        xdos_cpu_awake = ui_.AtDesktop() ? 0 : 1;
        if (xdos_cpu_awake) {
            scheduler_.Yield();
            XdosIdleUntilLvglDue();
        }
        else scheduler_.MsSleep(5);
    }
}

void CKernel::SDKLog(void *ctx,const char *message)
{
    CKernel *k=static_cast<CKernel*>(ctx);
    if(k) k->Debug(message);
}
int32_t CKernel::SDKShowDemo(void *ctx)
{
    (void)ctx;
    return -1;
}
int32_t CKernel::SDKShowBenchmark(void *ctx)
{
    (void)ctx;
    return -1;
}
