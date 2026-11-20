"""Remove the built-in benchmark from the generated XDOS core."""

from pathlib import Path
import re
import sys


source = Path(sys.argv[1])


def remove(name, *patterns):
    path = source / name
    text = path.read_text(encoding="utf-8")
    for pattern in patterns:
        text, count = re.subn(pattern, "", text, flags=re.MULTILINE)
        if count != 1:
            raise RuntimeError(f"{name}: expected one match for {pattern!r}, got {count}")
    path.write_text(text, encoding="utf-8")


remove("desktop.cpp",
       r",benchLaunchPending_\(false\)",
       r"^    if\(Contains\(x,y,151,97\+NWIN\*80,112,72\)\) \{benchLaunchPending_=true;return;\}\n",
       r"(?<=lvgl builtin) bench",
       r'^        else if\(!strcmp\(cmd_,"bench"\)\) \{benchLaunchPending_=true;[^\n]+\n',
       r'^    Fill\(160,shortcutY,[^\n]+\n    Frame\(160,shortcutY,[^\n]+\n'
       r'    Text\(173,shortcutY[^\n]+\n    Text\(151,shortcutY[^\n]+\n',
       r"^void Desktop::RequestBenchmarkLaunch\(\) \{ benchLaunchPending_=true; \}\n",
       r"^bool Desktop::TakeBenchmarkLaunchRequest\(\) \{[^}]+\}\n")
remove("desktop.h",
       r"^    void RequestBenchmarkLaunch\(\);\n",
       r"^    bool TakeBenchmarkLaunchRequest\(\);\n",
       r", benchLaunchPending_")
remove("xdos_lvgl.cpp",
       r"^void XDOSLVGL::BenchButton\(lv_event_t \*event\)\n\{[^}]+\}\n\n?",
       r"^bool XDOSLVGL::StartBenchmark\(\)\n\{[^}]+\}\n",
       r"^    benchmark_\.Tick\(nowUs\);\n",
       r"(?<=XDOSLVGL::Tick\()unsigned nowUs")
remove("xdos_lvgl.h",
       r'^#include "xdos_benchmark.h"\n',
       r"^    bool StartBenchmark\(\);\n",
       r"^    static void BenchButton\(lv_event_t \*event\);\n",
       r"^    XDOSBenchmark benchmark_;\n",
       r"(?<=void Tick\()unsigned nowUs")
remove("kernel.cpp",
       r"^        const bool openXdsBenchmark=[^\n]+\n"
       r"        if \(!uartOpen && openXdsBenchmark\) \{[^}]+\}\n",
       r"(?<=ui_\.Tick\()nowUs")
remove("Makefile", r" xdos_benchmark\.o\b")
