"""Keep the demo shortcuts and remove the built-in official LVGL demos."""

from pathlib import Path
import re
import sys

source = Path(sys.argv[1])
circle = Path(sys.argv[2])


def edit(file, before, after):
    p = source / file
    s = p.read_text(encoding="utf-8")
    if s.count(before) != 1:
        raise RuntimeError(f"Unexpected source layout in {file}: {before!r}")
    p.write_text(s.replace(before, after), encoding="utf-8")


edit("xdos_lvgl.h", "    bool StartWidgetsDemo();\n", "")
edit("xdos_lvgl.cpp", "#include <lvgl/lvgl/demos/lv_demos.h>\n", "")
edit("xdos_lvgl.cpp", "    if(ui) ui->StartWidgetsDemo();",
     "    if(ui && ui->desktop_) ui->desktop_->RequestLVGL();")
p = source / "xdos_lvgl.cpp"
text, count = re.subn(r"(?ms)^bool XDOSLVGL::StartWidgetsDemo\(\)\n\{\n.*?^\}\n", "",
                      p.read_text(encoding="utf-8"))
if count != 1:
    raise RuntimeError("Cannot remove built-in Widgets Demo")
p.write_text(text, encoding="utf-8")

config = circle / "addon/lvgl/lv_conf.h"
settings = config.read_text(encoding="utf-8")
for flag in ("LV_USE_DEMO_WIDGETS", "LV_USE_DEMO_BENCHMARK"):
    settings, count = re.subn(rf"(?m)^(\s*#define {flag}[ \t]+)[01]\b",
                              lambda match: match[1] + "0", settings)
    if count != 1:
        raise RuntimeError(f"Cannot disable {flag}")
if settings != config.read_text(encoding="utf-8"):
    config.write_text(settings, encoding="utf-8")

edit("desktop.h", "    bool TakeBenchmarkLaunchRequest();",
     "    bool TakeBenchmarkLaunchRequest();\n    bool TakeOfficialBenchmarkLaunchRequest();")
edit("desktop.h", "    bool dirty_, keyboard_, mouse_,", "    bool officialBenchPending_ = false;\n    bool dirty_, keyboard_, mouse_,")
edit("desktop.cpp", "    if(Contains(x,y,151,97+NWIN*80,112,72)) {benchLaunchPending_=true;return;}",
     """    if(Contains(x,y,151,97+NWIN*80,112,72)) {benchLaunchPending_=true;return;}
    if(Contains(x,y,286,97+NWIN*80,132,72)) {officialBenchPending_=true;return;}""")
edit("desktop.cpp", '    Text(151,shortcutY+38,"LVGL BENCH",WHITE);', '''    Text(151,shortcutY+38,"XDS BENCHMARK",WHITE);
    Fill(295,shortcutY,34,32,0x4D7E71);
    Frame(295,shortcutY,34,32,0xC0D4EA);
    Text(308,shortcutY+8,"B",WHITE);
    Text(286,shortcutY+38,"LVGL BENCHMARK",WHITE);''')
edit("desktop.cpp", "bool Desktop::TakeBenchmarkLaunchRequest() {", """bool Desktop::TakeOfficialBenchmarkLaunchRequest() {
    const bool pending = officialBenchPending_;
    officialBenchPending_ = false;
    return pending;
}

bool Desktop::TakeBenchmarkLaunchRequest() {""")
edit("desktop.cpp", "Starting kernel-built LVGL demo (XAPP bypass)...", "Loading external LVGLDEMO.XAPP...")
edit("desktop.cpp", "Starting LVGL Benchmark...", "Starting XDS Benchmark...")
edit("xdos_benchmark.cpp", "XD OS / LVGL 9 / PERFORMANCE BENCHMARK", "XDS BENCHMARK")

edit("kernel.cpp", """        if (openWidgets || openBuiltinWidgets) {
            desktop_.ReportAppResult(ui_.StartWidgetsDemo() ?
                "LVGL WIDGETS STARTED" : "LVGL WIDGETS BUSY");
        }""", """        if ((openWidgets || openBuiltinWidgets) && ui_.AtDesktop()) {
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunLVGLShortcut(&api) : "XAPP: FAT32 NOT READY");
        }
        if (desktop_.TakeOfficialBenchmarkLaunchRequest() && ui_.AtDesktop()) {
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunBenchmarkShortcut(&api) : "XAPP: FAT32 NOT READY");
        }""")
edit("kernel.cpp", '"LVGL BENCHMARK STARTED" : "LVGL BENCHMARK BUSY");\n#else',
     '"XDS BENCHMARK STARTED" : "XDS BENCHMARK BUSY");\n#else')
p = source / "kernel.cpp"
text = p.read_text(encoding="utf-8")
for hook in ("SDKShowDemo", "SDKShowBenchmark"):
    text, count = re.subn(rf"(?ms)^int32_t CKernel::{hook}\(void \*ctx\)\n\{{\n.*?^\}}",
                          f"int32_t CKernel::{hook}(void *ctx)\n{{\n    (void)ctx;\n    return -1;\n}}", text)
    if count != 1:
        raise RuntimeError(f"Cannot retire legacy built-in hook: {hook}")
p.write_text(text, encoding="utf-8")
