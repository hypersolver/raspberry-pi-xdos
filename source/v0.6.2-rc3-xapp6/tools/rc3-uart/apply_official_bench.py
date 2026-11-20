"""Enable the pinned LVGL Benchmark and connect it to the XDOS desktop."""

from pathlib import Path
import re
import sys


def replace(path, old, new):
    content = path.read_text(encoding="utf-8")
    if new in content:
        return
    if content.count(old) != 1:
        raise RuntimeError(f"Unexpected source layout in {path}: {old!r}")
    path.write_text(content.replace(old, new), encoding="utf-8")


source, circle = map(Path, sys.argv[1:3])
addon = circle / "addon/lvgl"
config = addon / "lv_conf.h"
content = config.read_text(encoding="utf-8")
for flag in ("LV_USE_SYSMON", "LV_USE_PERF_MONITOR", "LV_USE_DEMO_BENCHMARK",
             "LV_FONT_MONTSERRAT_26"):
    content, count = re.subn(rf"(?m)^(\s*#define {flag}[ \t]+)[01]\b",
                             lambda match: match[1] + "1", content)
    if count != 1:
        raise RuntimeError(f"Cannot configure {flag}")
if content != config.read_text(encoding="utf-8"):
    config.write_text(content, encoding="utf-8")

replace(addon / "Makefile", "CSRCS += $(shell find $(LVGL_PATH)/demos/widgets -type f -name *.c)",
        """CSRCS += $(shell find $(LVGL_PATH)/demos/widgets -type f -name '*.c')
CSRCS += $(shell find $(LVGL_PATH)/demos/benchmark -type f -name '*.c')""")

demo = addon / "lvgl/demos/benchmark/lv_demo_benchmark.c"
replace(demo, "static uint32_t scene_act;", """static lv_timer_t * xdos_scene_timer;
static lv_obj_t * xdos_benchmark_title;
static uint32_t scene_act;""")
replace(demo, "    scene_act = 0;", """    scene_act = 0;
    for(uint32_t i = 0; scenes[i].create_cb; ++i) {
        scenes[i].cpu_avg_usage = 0;
        scenes[i].fps_avg = 0;
        scenes[i].render_avg_time = 0;
        scenes[i].flush_avg_time = 0;
        scenes[i].measurement_cnt = 0;
    }""")
replace(demo, "    lv_obj_t * title = lv_label_create(lv_layer_top());",
        "    lv_obj_t * title = xdos_benchmark_title = lv_label_create(lv_layer_top());")
replace(demo, "    lv_timer_create(next_scene_timer_cb, scenes[0].scene_time, NULL);",
        "    xdos_scene_timer = lv_timer_create(next_scene_timer_cb, scenes[0].scene_time, NULL);")
replace(demo, "        lv_timer_delete(timer);", "        lv_timer_delete(timer);\n        xdos_scene_timer = NULL;")
replace(demo, "void lv_demo_benchmark_set_end_cb(", """void xdos_stop_official_benchmark(void)
{
    if(xdos_scene_timer) {
        lv_timer_delete(xdos_scene_timer);
        xdos_scene_timer = NULL;
    }
    if(xdos_benchmark_title) {
        lv_obj_delete(xdos_benchmark_title);
        xdos_benchmark_title = NULL;
    }
    lv_anim_delete(lv_layer_top(), color_anim_cb);
    lv_obj_set_style_bg_opa(lv_layer_top(), LV_OPA_TRANSP, 0);
}

void lv_demo_benchmark_set_end_cb(""")

desktop = source / "desktop.cpp"
desktop_header = source / "desktop.h"
replace(desktop_header, "    bool TakeBenchmarkLaunchRequest();",
        "    bool TakeBenchmarkLaunchRequest();\n    bool TakeOfficialBenchmarkLaunchRequest();")
replace(desktop_header, "    bool dirty_, keyboard_, mouse_,", "    bool officialBenchPending_ = false;\n    bool dirty_, keyboard_, mouse_,")
replace(desktop, "    if(Contains(x,y,151,97+NWIN*80,112,72)) {benchLaunchPending_=true;return;}",
        """    if(Contains(x,y,151,97+NWIN*80,112,72)) {benchLaunchPending_=true;return;}
#if XDOS_USE_LVGL
    if(Contains(x,y,286,97+NWIN*80,140,72)) {officialBenchPending_=true;return;}
#endif""")
replace(desktop, '    Text(151,shortcutY+38,"LVGL BENCH",WHITE);', '''    Text(151,shortcutY+38,"XDS BENCHMARK",WHITE);
#if XDOS_USE_LVGL
    Fill(295,shortcutY,34,32,0x4D7E71);
    Frame(295,shortcutY,34,32,0xC0D4EA);
    Text(308,shortcutY+8,"B",WHITE);
    Text(286,shortcutY+38,"LVGL BENCHMARK",WHITE);
#endif''')
replace(desktop, 'Starting LVGL Benchmark...', 'Starting XDS Benchmark...')
replace(desktop, "bool Desktop::TakeBenchmarkLaunchRequest() {", """bool Desktop::TakeOfficialBenchmarkLaunchRequest() {
    const bool request = officialBenchPending_;
    officialBenchPending_ = false;
    return request;
}

bool Desktop::TakeBenchmarkLaunchRequest() {""")
replace(source / "xdos_benchmark.cpp", "XD OS / LVGL 9 / PERFORMANCE BENCHMARK", "XDS BENCHMARK")

ui = source / "xdos_lvgl.cpp"
replace(source / "xdos_lvgl.h", "    bool StartBenchmark();",
        "    bool StartBenchmark();\n    bool StartOfficialBenchmark();")
replace(source / "xdos_lvgl.h", "    lv_obj_t *demoBack_ = nullptr;",
        "    lv_obj_t *demoBack_ = nullptr;\n    bool officialBenchmark_ = false;")
replace(ui, "#include <lvgl/lvgl/demos/lv_demos.h>",
        '#include <lvgl/lvgl/demos/lv_demos.h>\n\nextern "C" void xdos_stop_official_benchmark(void);')
replace(ui, "    lv_obj_delete_async(ui->demoBack_);", """    if (ui->officialBenchmark_) {
        lv_sysmon_performance_pause(lv_display_get_default());
        xdos_stop_official_benchmark();
        ui->officialBenchmark_ = false;
    }
    lv_obj_delete_async(ui->demoBack_);""")
replace(ui, "    ShowDesktop();", """    lv_sysmon_hide_performance(lv_display_get_default());
    lv_sysmon_performance_pause(lv_display_get_default());
    ShowDesktop();""")
replace(ui, "bool XDOSLVGL::StartWidgetsDemo()", """bool XDOSLVGL::StartOfficialBenchmark()
{
    if (!homeScreen_ || demoScreen_ || benchmark_.Running()) return false;
    SuspendMonitor();
    demoScreen_ = lv_obj_create(nullptr);
    if (!demoScreen_) {
        RestoreMonitor();
        return false;
    }
    officialBenchmark_ = true;
    lv_screen_load(demoScreen_);
    lv_sysmon_performance_resume(lv_display_get_default());
    lv_demo_benchmark();
    demoBack_ = MakeButton(lv_layer_top(),"BACK TO DESKTOP",DemoBack,this);
    lv_obj_set_size(demoBack_,168,37);
    lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    if (serial_) {
        static const char message[] = "[LVGL BENCH] Official benchmark started\\r\\n";
        serial_->Write(message, sizeof message - 1);
    }
    return true;
}

bool XDOSLVGL::StartWidgetsDemo()""")
replace(ui, "    if(demoScreen_) return true;", "    if(demoScreen_) return !officialBenchmark_;")

replace(source / "kernel.cpp", "        ui_.Tick(nowUs);", """        if (desktop_.TakeOfficialBenchmarkLaunchRequest())
            desktop_.ReportAppResult(ui_.StartOfficialBenchmark() ?
                "LVGL BENCHMARK STARTED" : "LVGL BENCHMARK BUSY");
        ui_.Tick(nowUs);""")
replace(source / "kernel.cpp", '"LVGL BENCHMARK STARTED" : "LVGL BENCHMARK BUSY");\n#else',
        '"XDS BENCHMARK STARTED" : "XDS BENCHMARK BUSY");\n#else')
