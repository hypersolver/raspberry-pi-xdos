"""Apply boot, desktop, and UART performance changes to the patched XAPP6 source."""

from pathlib import Path
import sys


source = Path(sys.argv[1])
circle = Path(sys.argv[2])
files = {}


def edit(name, old, new):
    path = source / name
    if path not in files:
        files[path] = path.read_text()
    text = files[path]
    if text.count(old) != 1:
        raise RuntimeError(f"{name}: expected one occurrence of {old!r}")
    files[path] = text.replace(old, new)


edit("kernel.h", "    CUSBHCIDevice usb_;", """    CUSBHCIDevice usb_;
    unsigned bootStartUs_ = 0;
    bool bootNetworkLogged_ = false;
    void LogBootTiming(const char *stage);""")
edit("kernel.cpp", '#include "p0_metrics.h"', '#include "p0_metrics.h"\n#include <circle/xdos_test_debug.h>')
edit("kernel.cpp", "boolean CKernel::Initialize()\n{", """void CKernel::LogBootTiming(const char *stage)
{
#if XDOS_TEST_DEBUG
    CString line;
    line.Format("[BOOT PERF] %s=%u us\\r\\n",stage,
                (unsigned)(CTimer::GetClockTicks()-bootStartUs_));
    Debug((const char *)line);
#else
    (void)stage;
#endif
}

boolean CKernel::Initialize()
{
    bootStartUs_=CTimer::GetClockTicks();""")
edit("kernel.cpp", """    if(!usb_.Initialize()) {
        Boot(6,"USB HOST","initialization failed",CBootSplash::ERROR);
        return FALSE;
    }
    Boot(6,"USB HOST","hub/controller initialized; HID enumeration deferred");""",
     """    Boot(6,"USB HOST","starts after first desktop frame",CBootSplash::PENDING);""")
edit("kernel.cpp", "    scheduler_.MsSleep(150);\n\n    desktop_.Print", "\n    desktop_.Print")
edit("kernel.cpp", '''            desktop_.SetStorage(storageMounted_,storageMounted_?"FAT32: READY":"FAT32: MOUNT FAILED");''',
     '''            desktop_.SetStorage(storageMounted_,storageMounted_?"FAT32: READY":"FAT32: MOUNT FAILED");
            if (storageMounted_) LogBootTiming("fat32_ready");''')
edit("kernel.cpp", """    if(!ip)return;
    CString state;""", """    if(!ip)return;
    if (!bootNetworkLogged_ && (ip[0] || ip[1] || ip[2] || ip[3])) {
        LogBootTiming("network_ready");
        bootNetworkLogged_=true;
    }
    CString state;""")
edit("kernel.cpp", '    logger_.Write("xdos",LogNotice,"XDOS V0.6.2-RC3 XAPP6: LVGL P0 + USB MSC / ARM32 Pi3");',
     '    Debug("[XDOS] V0.6.2-RC3 XAPP6-PERF3 / Pi 3 ARM32\\r\\n");')
edit("kernel.cpp", "logger_(options_.GetLogLevel(),&timer_)",
     "logger_(options_.GetLogLevel()<LogWarning ? options_.GetLogLevel() : LogWarning,&timer_)")
edit("kernel.cpp", "    if (boot_.GetLineCount()) {",
     """    if ((status==CBootSplash::WARNING || status==CBootSplash::ERROR) &&
        boot_.GetLineCount()) {""")
for message in (
    "Deferred network initialization started",
    "Ethernet link up; starting DHCP",
    "No wired link; loading SDIO WiFi firmware",
    "WLAN firmware initialized",
    "WiFi unavailable; using Ethernet fallback",
    "HTTP worker started; waiting for DHCP",
):
    path = source / "kernel.cpp"
    line = f'Debug("[NET] {message}\\r\\n");'
    matches = [entry for entry in files[path].splitlines() if entry.strip() == line]
    if len(matches) != 1:
        raise RuntimeError(f"kernel.cpp: expected single network message {message!r}")
    edit("kernel.cpp", matches[0] + "\n", "")
edit("kernel.cpp", '            if (storageMounted_) LogBootTiming("fat32_ready");',
     '''            if (storageMounted_) LogBootTiming("fat32_ready");
            else Debug("[STORAGE ERROR] FAT32 mount failed\\r\\n");''')
edit("kernel.cpp", '            desktop_.SetStorage(false,"FAT32: SD HOST FAILED");',
     '''            desktop_.SetStorage(false,"FAT32: SD HOST FAILED");
            Debug("[STORAGE ERROR] SD host initialization failed\\r\\n");''')
edit("kernel.cpp", '    unsigned perfLabels = ui_.TextUpdateCount();',
     '''    unsigned perfLabels = ui_.TextUpdateCount();
    unsigned perfDmaFlushes = xdos_p0_dma_completed;
    unsigned perfCpuFlushes = xdos_p0_cpu_flushes;''')
edit("kernel.cpp", """            CString logLine;
            const unsigned labels = ui_.TextUpdateCount();""",
     """            const unsigned labels = ui_.TextUpdateCount();""")
edit("kernel.cpp", """            logLine.Format("[LVGL P0] update_avg=%u us max=%u us calls=%u labels=%u wfi_idle=%u%% DMA=%u CPUflush=%u\\r\\n",
                           perfCalls ? (unsigned)(perfTotalUs / perfCalls) : 0U,
                           perfMaxUs, perfCalls, labels - perfLabels, idlePct,
                           xdos_p0_dma_completed, xdos_p0_cpu_flushes);
            ui_.UpdateMonitor(100U - idlePct);
            idleBaseUs=xdos_wfi_microseconds;
            idleBaseTicks=nowUs;
            Debug((const char *)logLine);""",
     """            const unsigned dmaFlushes = xdos_p0_dma_completed;
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
            idleBaseTicks=nowUs;""")
edit("kernel.cpp", """    // Boot screen has already been replaced with a usable, empty desktop.
    // Only now start network discovery as a cooperative task.
    // USB Mass Storage auto-mount runs as a deferred Circle task. It does not
    // delay the first desktop frame or require Ethernet/WLAN to be ready.
    if(!storageTask_)storageTask_=new CStorageTask(&desktop_);
    new CNetworkInitTask(this);
    xdos_arduino_attach_serial(debugSerialReady_ ? &serial_ : 0);
    new CArduinoSketchTask();
#if XDOS_USE_LVGL
    ui_.Initialize(&desktop_,debugSerialReady_ ? &serial_ : 0);
#endif""", """#if XDOS_USE_LVGL
    ui_.Initialize(&desktop_,debugSerialReady_ ? &serial_ : 0);
    lv_refr_now(lv_display_get_default());
    const unsigned flushStartUs=CTimer::GetClockTicks();
    while (xdos_p0_dma_submitted != xdos_p0_dma_completed &&
           (unsigned)(CTimer::GetClockTicks()-flushStartUs) < 100000U) {
        scheduler_.MsSleep(1);
    }
    LogBootTiming(xdos_p0_dma_submitted == xdos_p0_dma_completed ?
                  "desktop_first_flush" : "desktop_flush_timeout");
#endif
    LogBootTiming("usb_start");
    if (!usb_.Initialize()) {
        Debug("[BOOT ERROR] USB HOST initialization failed\\r\\n");
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
    new CArduinoSketchTask();""")

edit("desktop.h", "    void ClearDamage();", """    void ClearDamage();
    void RenderDamage(void *framebuffer,unsigned pitchBytes,unsigned depthBits,
                      int x,int y,int w,int h);""")
edit("desktop.h", "    void DragDamage(", """    void MarkDamage(int x0,int y0,int x1,int y1);
    bool clipRender_ = false;
    int clipX0_ = 0, clipY0_ = 0, clipX1_ = 0, clipY1_ = 0;
    void DragDamage(""")

# A full state change must supersede partial damage already queued by a drag.
path = source / "desktop.cpp"
text = path.read_text()
drag_start = text.index("void Desktop::DragDamage")
drag_end = text.index("void Desktop::GetDamage", drag_start)
full_repaint = "void Desktop::RequestFullRepaint() { dirty_=true;damagePartial_=false; }"
if text.count(full_repaint) != 1:
    raise RuntimeError("desktop.cpp: full repaint anchor changed")
files[path] = (
    text[:drag_start].replace("dirty_=true;", "RequestFullRepaint();")
    + text[drag_start:drag_end]
    + text[drag_end:].replace("dirty_=true;", "RequestFullRepaint();")
      .replace("void Desktop::RequestFullRepaint() { RequestFullRepaint();damagePartial_=false; }",
               full_repaint)
)
edit("desktop.cpp", """    if(x0<0)x0=0;
    if(y0<0)y0=0;""", """    MarkDamage(x0,y0,x1,y1);
}
void Desktop::MarkDamage(int x0,int y0,int x1,int y1)
{
    if(x0<0)x0=0;
    if(y0<0)y0=0;""")
edit("desktop.cpp", "    if(y1>height_)y1=height_;\n    if(!dirty_)",
     "    if(y1>height_)y1=height_;\n    if(x0>=x1 || y0>=y1)return;\n    if(!dirty_)")
edit("desktop.cpp", """{ if (keyboard_!=keyboard||mouse_!=mouse) RequestFullRepaint(); keyboard_=keyboard; mouse_=mouse; }""",
     """{
    if (keyboard_!=keyboard || mouse_!=mouse) MarkDamage(width_-248,78,width_-20,144);
    keyboard_=keyboard;
    mouse_=mouse;
}""")
edit("desktop.cpp", """        netStatus_[sizeof(netStatus_)-1]=0;
        RequestFullRepaint();""", """        netStatus_[sizeof(netStatus_)-1]=0;
        MarkDamage(width_-248,78,width_-20,144);""")
edit("desktop.cpp", """    if(!ready) strncpy(filesContent_,"FAT32 volume not available.\\n",sizeof(filesContent_)-1);
    RequestFullRepaint();""", """    if(!ready) strncpy(filesContent_,"FAT32 volume not available.\\n",sizeof(filesContent_)-1);
    if (windows_[Files].open) RequestFullRepaint();
    else MarkDamage(22,height_-95,width_,height_-79);""")
edit("desktop.cpp", """    // Translation of the original x86 rect + clip to AArch32 assembly.
    // Addressing honors the real GPU pitch, including padding.""", """    if (clipRender_) {
        const int right=Clamp(x+w,clipX0_,clipX1_);
        const int bottom=Clamp(y+h,clipY0_,clipY1_);
        x=Clamp(x,clipX0_,clipX1_);
        y=Clamp(y,clipY0_,clipY1_);
        w=right-x;
        h=bottom-y;
        if (w<=0 || h<=0) return;
    }
    // Addressing honors the real GPU pitch, including padding.""")
edit("desktop.cpp", """    while(*s) {
        unsigned char c=(unsigned char)*s++;""", """    if (clipRender_ && (y+16<=clipY0_ || y>=clipY1_)) return;
    while(*s) {
        if (clipRender_ && x>=clipX1_) break;
        unsigned char c=(unsigned char)*s++;
        if (clipRender_ && x+8<=clipX0_) {
            x+=8;
            continue;
        }""")
edit("desktop.cpp", """            for(unsigned xx=0;xx<8;xx++)
                if(font.GetPixel(xx,line)) mask|=(unsigned short)(1u<<(7-xx));""", """            if (!clipRender_ || (y+(int)yy>=clipY0_ && y+(int)yy<clipY1_)) {
                for(unsigned xx=0;xx<8;xx++)
                    if ((!clipRender_ || (x+(int)xx>=clipX0_ && x+(int)xx<clipX1_)) &&
                        font.GetPixel(xx,line)) mask|=(unsigned short)(1u<<(7-xx));
            }""")
edit("desktop.cpp", """    for(int y=0;y<height_-49;y++) {""", """    const int firstRow=clipRender_ ? clipY0_ : 0;
    const int lastRow=clipRender_ ? Clamp(clipY1_,0,height_-49) : height_-49;
    for(int y=firstRow;y<lastRow;y++) {""")
edit("desktop.cpp", """// Optional LVGL mode; switching does not reset existing app state.""", """void Desktop::RenderDamage(void *fb,unsigned pitch,unsigned depth,int x,int y,int w,int h)
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

// Optional LVGL mode; switching does not reset existing app state.""")
edit("xdos_lvgl.cpp", """    desktop_->Render(desktopPixels_,desktop_->Width()*2,16);
    desktop_->ClearDamage();
    lv_obj_invalidate(desktopCanvas_);""", """    int x,y,w,h;
    desktop_->GetDamage(x,y,w,h);
    desktop_->RenderDamage(desktopPixels_,desktop_->Width()*2,16,x,y,w,h);
    desktop_->ClearDamage();
    if (x==0 && y==0 && w==desktop_->Width() && h==desktop_->Height()) {
        lv_obj_invalidate(desktopCanvas_);
        return;
    }
    lv_area_t canvas;
    lv_obj_get_coords(desktopCanvas_,&canvas);
    const lv_area_t dirty={canvas.x1+x,canvas.y1+y,canvas.x1+x+w-1,canvas.y1+y+h-1};
    lv_obj_invalidate_area(desktopCanvas_,&dirty);""")

edit(circle / "lib/net/nettask.cpp",
     "\t\tCScheduler::Get ()->Yield ();",
     """		// This polling task must become non-runnable between passes so
		// Circle can enter WFI when all other tasks are idle.
		CScheduler::Get ()->MsSleep (1);""")
edit("xapp_loader.h",
     "static Result ExecuteFile(const char *path,const XdosHostApiV1 *api);",
     "static Result ExecuteFile(const char *path,const XdosHostApiV1 *api,bool requireBound=false);")
edit("xapp_loader.cpp",
     "XappLoaderV1::ExecuteFile(const char *path, const XdosHostApiV1 *api)",
     "XappLoaderV1::ExecuteFile(const char *path, const XdosHostApiV1 *api, bool requireBound)")
edit("xapp_loader.cpp", "        h->flags > 1 ||",
     "        h->flags > 1 || (requireBound && h->flags != 1) ||")
edit("app_shortcut.cpp", """const char *XdosRunLVGLShortcut(const XdosHostApiV1 *api)
{
    if(!VerifyManifest(kManifest,kTrustedModule))return "XAPP: LVGLDEMO.APP missing or invalid";
    switch(XappLoaderV1::ExecuteFile(kTrustedModule,api)) {""",
     """static XappLoaderV1::Result RunBoundDemo(const char *manifest,const char *module,
                                            const char *staged,const XdosHostApiV1 *api)
{
    // UART updates can stage a build-specific pair without overwriting the
    // previous release's files. Both paths require the active kernel fingerprint.
    const auto result=XappLoaderV1::ExecuteFile(staged,api,true);
    if (result!=XappLoaderV1::OPEN_FAILED) return result;
    if (!VerifyManifest(manifest,module)) return XappLoaderV1::FORMAT_INVALID;
    return XappLoaderV1::ExecuteFile(module,api,true);
}

const char *XdosRunLVGLShortcut(const XdosHostApiV1 *api)
{
    switch(RunBoundDemo(kManifest,kTrustedModule,"SD:/XDOS/INBOX/P3WIDGET.XAPP",api)) {""")
edit("app_shortcut.cpp", """    if (!VerifyManifest(kBenchManifest,kBenchModule))
        return "XAPP: LVGLBENCH.APP missing or invalid (SD:/XDOS/APPS/)";
    switch(XappLoaderV1::ExecuteFile(kBenchModule,api)) {""",
     """    switch(RunBoundDemo(kBenchManifest,kBenchModule,"SD:/XDOS/INBOX/P3BENCH.XAPP",api)) {""")

for path, text in files.items():
    path.write_text(text, encoding="utf-8")
