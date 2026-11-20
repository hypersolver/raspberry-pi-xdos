from pathlib import Path
import shutil
import sys


def replace(file, before, after):
    path = source / file
    text = path.read_text()
    if after in text:
        return
    if text.count(before) != 1:
        raise RuntimeError(f"Expected one patch anchor in {file}: {before!r}")
    path.write_text(text.replace(before, after), encoding="utf-8")


source = Path(sys.argv[1])
replace("kernel.cpp", '#include "kernel.h"', '#include "kernel.h"\n#include "uart_service.h"')
replace("kernel.h", """    CSerialDevice serial_;
    CExceptionHandler exception_;
    CInterruptSystem interrupt_;""", """    CExceptionHandler exception_;
    CInterruptSystem interrupt_;
    CSerialDevice serial_;""")
replace("kernel.cpp", "  ), timer_(&interrupt_),", "  ), serial_(&interrupt_), timer_(&interrupt_),")
replace("kernel.cpp", """    // UART first. Even if HDMI cannot initialize, early failure can be logged.
    debugSerialReady_=serial_.Initialize(115200);""", """    // Initialize IRQ before the interrupt-backed UART receive buffer.
    const boolean irqReady = interrupt_.Initialize();
    debugSerialReady_=irqReady && serial_.Initialize(115200);""")
replace("kernel.cpp", "if(!interrupt_.Initialize()) {", "if(!irqReady) {")
replace("kernel.cpp", "    while(1) {\n        boolean plugUpdated", """    static XdosUartService uart;
    Debug("[XDOS UART] READY 1\\r\\n");
    while(1) {
        if (debugSerialReady_) uart.Poll(serial_, storageMounted_);
        boolean plugUpdated""")
replace("kernel.cpp", "XDOS V0.6.2-RC3: LVGL P0", "XDOS V0.6.2-RC3 XAPP6: LVGL P0")
replace("kernel.cpp", "            idleBaseUs=xdos_wfi_microseconds;",
        "            ui_.UpdateMonitor(100U - idlePct);\n            idleBaseUs=xdos_wfi_microseconds;")
replace("kernel.cpp", """        if (desktop_.TakeAppLaunchRequest()) {
            desktop_.ReportAppResult(storageMounted_ ?""", """        const bool openWidgets = desktop_.TakeAppLaunchRequest();
#if XDOS_USE_LVGL
        const bool openBuiltinWidgets = desktop_.TakeLVGLRequest();
        if (openWidgets || openBuiltinWidgets) {
            desktop_.ReportAppResult(ui_.StartWidgetsDemo() ?
                "LVGL WIDGETS STARTED" : "LVGL WIDGETS BUSY");
        }
#else
        if (openWidgets) {
            desktop_.ReportAppResult(storageMounted_ ?""")
replace("kernel.cpp", """                XdosRunLVGLShortcut(&api) : "XAPP: FAT32 NOT READY");
        }""", """                XdosRunLVGLShortcut(&api) : "XAPP: FAT32 NOT READY");
        }
#endif""")
replace("kernel.cpp", """            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunBenchmarkShortcut(&api) : "XAPP: FAT32 NOT READY");""", """#if XDOS_USE_LVGL
            desktop_.ReportAppResult(ui_.StartBenchmark() ?
                "LVGL BENCHMARK STARTED" : "LVGL BENCHMARK BUSY");
#else
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunBenchmarkShortcut(&api) : "XAPP: FAT32 NOT READY");
#endif""")
replace("xdos_lvgl.h", "    unsigned lastUsbStorageRevision_;",
        "    lv_obj_t *demoBack_ = nullptr;\n    unsigned lastUsbStorageRevision_;")
replace("xdos_lvgl.cpp", "    lv_screen_load(ui->homeScreen_);",
        """    lv_obj_delete_async(ui->demoBack_);
    ui->demoBack_=nullptr;
    lv_screen_load(ui->homeScreen_);""")
replace("xdos_lvgl.cpp", "if(!homeScreen_ || benchmark_.Running()) return false;\n    return benchmark_.Start",
        "if(!homeScreen_ || demoScreen_ || benchmark_.Running()) return false;\n    return benchmark_.Start")
replace("xdos_lvgl.cpp", '    lv_obj_t *back=MakeButton(demoScreen_,"BACK XD OS",DemoBack,this);',
        '    lv_obj_t *back=demoBack_=MakeButton(lv_layer_top(),"BACK TO DESKTOP",DemoBack,this);')
replace("xdos_lvgl.cpp", "    lv_obj_set_size(back,116,37);", "    lv_obj_set_size(back,168,37);")
replace("xdos_benchmark.cpp", 'lv_label_set_text(bLabel,"BACK / STOP")',
        'lv_label_set_text(bLabel,"BACK TO DESKTOP")')
replace("xdos_benchmark.cpp", "lv_obj_set_size(back,125,36);", "lv_obj_set_size(back,168,36);")
replace("xdos_benchmark.cpp", "CLICK BACK / STOP", "CLICK BACK TO DESKTOP")
replace("xdos_benchmark.cpp", """    lv_obj_set_size(area_,lv_pct(98),lv_pct(78));
    lv_obj_align(area_,LV_ALIGN_TOP_MID,0,82);""", """    // Keep the measurement canvas bounded inside the full-screen controls.
    lv_obj_set_size(area_,lv_pct(92),lv_pct(72));
    lv_obj_set_style_max_width(area_,640,0);
    lv_obj_set_style_max_height(area_,420,0);
    lv_obj_remove_flag(area_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(area_,LV_ALIGN_TOP_MID,0,82);""")
replace("xdos_benchmark.cpp", 'BENCHMARK COMPLETED - CLICK BACK TO DESKTOP',
        'BENCHMARK COMPLETED')
replace("xdos_lvgl.h", "    static void BenchButton(lv_event_t *event);", """    static void BenchButton(lv_event_t *event);
    static void DesktopPointer(lv_event_t *event);
    static void DesktopLoaded(lv_event_t *event);
    void ShowDesktop();
    void SuspendMonitor();
    void RestoreMonitor();
    lv_obj_t *desktopCanvas_ = nullptr;
    lv_obj_t *desktopMonitor_ = nullptr;
    unsigned lastCpuPercent_ = 101;
    uint16_t *desktopPixels_ = nullptr;""")
replace("xdos_lvgl.h", "    void OnKey(char ch);", "    void OnKey(char ch);\n    void UpdateMonitor(unsigned cpuPercent);")
replace("xdos_lvgl.cpp", "void XDOSLVGL::Initialize(Desktop *desktop,CSerialDevice *serial)", """void XDOSLVGL::DesktopLoaded(lv_event_t *event)
{
    static_cast<XDOSLVGL *>(lv_event_get_user_data(event))->RestoreMonitor();
}

void XDOSLVGL::SuspendMonitor()
{
    if (desktopMonitor_) lv_obj_add_flag(desktopMonitor_, LV_OBJ_FLAG_HIDDEN);
}

void XDOSLVGL::RestoreMonitor()
{
    if (desktopMonitor_) lv_obj_remove_flag(desktopMonitor_, LV_OBJ_FLAG_HIDDEN);
}

void XDOSLVGL::UpdateMonitor(unsigned cpuPercent)
{
    if (!desktopMonitor_ || lv_screen_active() != homeScreen_ ||
        lv_obj_has_flag(desktopMonitor_, LV_OBJ_FLAG_HIDDEN) ||
        lastCpuPercent_ == cpuPercent) return;
    lv_label_set_text_fmt(desktopMonitor_, "CPU %u%%", cpuPercent);
    lastCpuPercent_ = cpuPercent;
}

void XDOSLVGL::DesktopPointer(lv_event_t *event)
{
    auto *ui = static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    auto *input = lv_indev_active();
    if (!input) return;
    lv_point_t pt;
    lv_indev_get_point(input, &pt);
    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED: ui->desktop_->Mouse(Desktop::Down,1,pt.x,pt.y); break;
    case LV_EVENT_PRESSING: ui->desktop_->Mouse(Desktop::Move,1,pt.x,pt.y); break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: ui->desktop_->Mouse(Desktop::Up,0,pt.x,pt.y); break;
    default: break;
    }
}

void XDOSLVGL::ShowDesktop()
{
    if (!desktopCanvas_) {
        const int w = desktop_->Width(), h = desktop_->Height();
        desktopPixels_ = new uint16_t[static_cast<unsigned>(w) * h];
        if (!desktopPixels_) {
            desktop_->ReportAppResult("DESKTOP BUFFER UNAVAILABLE");
            return;
        }
        lv_obj_set_style_pad_all(homeScreen_,0,0);
        lv_obj_remove_flag(homeScreen_,LV_OBJ_FLAG_SCROLLABLE);
        desktopCanvas_ = lv_canvas_create(homeScreen_);
        lv_canvas_set_buffer(desktopCanvas_,desktopPixels_,w,h,LV_COLOR_FORMAT_RGB565);
        lv_obj_add_flag(desktopCanvas_,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(desktopCanvas_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(desktopCanvas_,DesktopPointer,LV_EVENT_ALL,this);
#if XDOS_LVGL_PERF
        desktopMonitor_ = lv_label_create(homeScreen_);
        lv_label_set_text(desktopMonitor_, "CPU --%");
        lv_obj_set_style_text_color(desktopMonitor_,lv_color_white(),0);
        lv_obj_align(desktopMonitor_,LV_ALIGN_TOP_RIGHT,-12,8);
        lv_obj_add_event_cb(homeScreen_,DesktopLoaded,LV_EVENT_SCREEN_LOADED,this);
#endif
    }
    desktop_->RequestFullRepaint();
    Refresh(0);
}

void XDOSLVGL::Initialize(Desktop *desktop,CSerialDevice *serial)""")
ui_path = source / "xdos_lvgl.cpp"
ui = ui_path.read_text(encoding="utf-8")
start = ui.index("    lv_obj_set_style_bg_color(homeScreen_")
end = ui.index("\n}\n", start)
ui_path.write_text(ui[:start] + "    ShowDesktop();" + ui[end:], encoding="utf-8")
ui = ui_path.read_text(encoding="utf-8")
start = ui.index("void XDOSLVGL::Refresh(")
ui_path.write_text(ui[:start] + """void XDOSLVGL::Refresh(unsigned)
{
    if (!desktopCanvas_ || lv_screen_active() != homeScreen_ ||
        !desktop_->NeedsRedraw()) return;
    desktop_->Render(desktopPixels_,desktop_->Width()*2,16);
    desktop_->ClearDamage();
    lv_obj_invalidate(desktopCanvas_);
}
""", encoding="utf-8")
replace("desktop.cpp", 'Loading LVGLBENCH.XAPP from FAT32...', 'Starting LVGL Benchmark...')
replace("desktop.cpp", 'Loading LVGLDEMO.XAPP from FAT32...', 'Starting LVGL Widgets...')
replace("xdos_lvgl.cpp", "    return benchmark_.Start(homeScreen_,desktop_,serial_);",
        """    SuspendMonitor();
    if (benchmark_.Start(homeScreen_,desktop_,serial_)) return true;
    RestoreMonitor();
    return false;""")
replace("xdos_lvgl.cpp", "    demoScreen_=lv_obj_create(NULL);",
        """    SuspendMonitor();
    demoScreen_=lv_obj_create(NULL);
    if (!demoScreen_) {
        RestoreMonitor();
        return false;
    }""")
shutil.copyfile(Path(__file__).with_name("uart_service.h"), source / "uart_service.h")
