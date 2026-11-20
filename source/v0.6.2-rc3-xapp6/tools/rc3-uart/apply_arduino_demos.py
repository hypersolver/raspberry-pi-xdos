"""Add kernel-paired code loading and a host-owned Arduino display session."""

from pathlib import Path
import shutil
import sys

source, kit = map(Path, sys.argv[1:])


def edit(path, before, after):
    data = path.read_text(encoding="utf-8")
    if data.count(before) != 1:
        raise RuntimeError(f"Unexpected source layout in {path}: {before!r}")
    path.write_text(data.replace(before, after), encoding="utf-8")


shutil.copyfile(Path(__file__).with_name("xapp_arduino_lvgl.h"),
                source / "xapp_arduino_lvgl.h")
edit(source / "xapp_exec_slot.S", ".space 65536, 0", ".space 327680, 0")
edit(source / "xdos_sdk_v1.h", "#define XDOS_XAPP_MAX_SIZE (64u*1024u)",
     "#define XDOS_XAPP_MAX_SIZE (320u*1024u)")
edit(kit / "tools/check-exec-section.py", "end-start!=65536", "end-start!=327680")
edit(source / "Makefile", "include $(CIRCLEHOME)/Rules.mk",
     "include xapp-imports.mk\ninclude $(CIRCLEHOME)/Rules.mk")

loader = source / "xapp_loader.cpp"
edit(loader, 'extern "C" unsigned char xdos_xapp_exec_slot_end[];',
    '''extern "C" unsigned char xdos_xapp_exec_slot_end[];
extern "C" const unsigned char _start[], _etext[];

static uint32_t KernelFingerprint()
{
    static bool computed;
    static uint32_t fingerprint;
    if (computed) return fingerprint;
    uint32_t crc = ~0U;
    const auto first = reinterpret_cast<uintptr_t>(_start);
    const auto slot = reinterpret_cast<uintptr_t>(xdos_xapp_exec_slot);
    const auto after = reinterpret_cast<uintptr_t>(xdos_xapp_exec_slot_end);
    const auto end = reinterpret_cast<uintptr_t>(_etext);
    for (auto address = first; address < end; ++address) {
        if (address == slot) address = after;
        if (address == end) break;
        crc ^= *reinterpret_cast<const unsigned char *>(address);
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0U);
    }
    fingerprint = ~crc;
    computed = true;
    return fingerprint;
}''')
edit(loader, "h->image_bytes != (uint32_t)fileSize || h->flags ||\n        h->checksum || h->reserved ||",
     """h->image_bytes != (uint32_t)fileSize ||
        (h->flags == 0 && (h->checksum || h->reserved || fileSize > 64U*1024)) ||
        h->flags > 1 ||""")
edit(loader, "    const uint32_t entryOffset=h->entry_offset;", """    if (h->flags == 1 &&
        (h->reserved != reinterpret_cast<uintptr_t>(xdos_xapp_exec_slot) ||
         h->checksum != KernelFingerprint())) {
        delete [] image;
        Trace(api,"[XAPP] Module belongs to a different kernel build\\r\\n");
        return FORMAT_INVALID;
    }
    const uint32_t entryOffset=h->entry_offset;
    memset(xdos_xapp_exec_slot,0,slotSize);""")

header = source / "xdos_lvgl.h"
ui = source / "xdos_lvgl.cpp"
edit(header, '#include "xapp_workspace.h"', '#include "xapp_workspace.h"\n#include "xapp_arduino_lvgl.h"')
edit(header, "    bool AtDesktop() const;", """    bool AtDesktop() const;
    int32_t RunArduinoSketch(XdosSketchRoutine setup, XdosSketchRoutine loop,
                             XdosSketchRoutine stop, bool measurements);""")
edit(header, "    void PaintEvent(int event, int x, int y);", """    void PaintEvent(int event, int x, int y);
    XdosSketchRoutine sketchLoop_ = nullptr, sketchStop_ = nullptr;
    bool sketchActive_ = false, sketchMeasurements_ = false;""")
edit(ui, '#include "xdos_lvgl.h"', '#include "xdos_lvgl.h"\n#include <circle/xdos_test_debug.h>')
edit(ui, "    if (ui->workspacePanel_ == XDOS_PANEL_HTTP)", """    if (ui->sketchActive_) {
        ui->sketchLoop_ = nullptr;
        if (ui->sketchMeasurements_) lv_sysmon_performance_pause(lv_display_get_default());
        if (ui->sketchStop_) ui->sketchStop_();
        ui->sketchStop_ = nullptr;
        ui->sketchActive_ = false;
        ui->sketchMeasurements_ = false;
        auto *screen = ui->demoScreen_;
        ui->demoScreen_ = nullptr;
        lv_obj_delete_async(ui->demoBack_);
        ui->demoBack_ = nullptr;
        lv_screen_load(ui->homeScreen_);
        // App-owned object delete callbacks must run before the slot is reused.
        lv_obj_delete(screen);
        if (XDOS_TEST_DEBUG && ui->serial_) {
            static const char message[] = "[XAPP] Arduino LVGL session closed\\r\\n";
            ui->serial_->Write(message,sizeof message-1);
        }
        return;
    }
    if (ui->workspacePanel_ == XDOS_PANEL_HTTP)""")
edit(ui, "    ShowDesktop();", """    lv_sysmon_hide_performance(lv_display_get_default());
    lv_sysmon_performance_pause(lv_display_get_default());
    ShowDesktop();""")
edit(ui, "    if (workspaceAction_) UpdateWorkspace();",
      "    if (workspaceAction_) UpdateWorkspace();\n    if (sketchLoop_) sketchLoop_();")
edit(ui, "void XDOSLVGL::Refresh(unsigned)", """int32_t XDOSLVGL::RunArduinoSketch(XdosSketchRoutine setup, XdosSketchRoutine loop,
                                     XdosSketchRoutine stop, bool measurements)
{
    if (!AtDesktop() || demoScreen_ || !setup || !loop) return -1;
    demoScreen_ = lv_obj_create(nullptr);
    if (!demoScreen_) return -1;
    SuspendMonitor();
    sketchActive_ = true;
    sketchMeasurements_ = measurements;
    sketchLoop_ = loop;
    sketchStop_ = stop;
    lv_screen_load(demoScreen_);
    if (measurements) lv_sysmon_performance_resume(lv_display_get_default());
    setup();
    if (XDOS_TEST_DEBUG && serial_) {
        static const char message[] = "[XAPP] Arduino LVGL setup completed\\r\\n";
        serial_->Write(message,sizeof message-1);
    }
    demoBack_ = MakeButton(lv_layer_top(),"BACK TO DESKTOP",DemoBack,this);
    lv_obj_set_size(demoBack_,168,37);
    lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    return 0;
}

void XDOSLVGL::Refresh(unsigned)""")
kernel = source / "kernel.cpp"
edit(kernel, "        const bool openBuiltinWidgets = desktop_.TakeLVGLRequest();", """        XdosArduinoLvglApi sketch = {api, [](void *ctx, XdosSketchRoutine setup,
                                             XdosSketchRoutine loop, XdosSketchRoutine stop,
                                             int measured) -> int32_t {
            return static_cast<CKernel *>(ctx)->ui_.RunArduinoSketch(setup,loop,stop,measured);
        }};
        sketch.base.struct_bytes = sizeof sketch;
        const bool openBuiltinWidgets = desktop_.TakeLVGLRequest();""")
edit(kernel, 'XdosRunLVGLShortcut(&api) : "XAPP: FAT32 NOT READY");\n        }\n        if (desktop_.TakeOfficialBenchmarkLaunchRequest()',
    'XdosRunLVGLShortcut(&sketch.base) : "XAPP: FAT32 NOT READY");\n        }\n        if (desktop_.TakeOfficialBenchmarkLaunchRequest()')
edit(kernel, """                XdosRunBenchmarkShortcut(&api) : "XAPP: FAT32 NOT READY");
        }
#else""", """                XdosRunBenchmarkShortcut(&sketch.base) : "XAPP: FAT32 NOT READY");
        }
#else""")
