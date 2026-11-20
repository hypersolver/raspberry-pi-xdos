"""Limit GPU reuse to the official static-card scrolling benchmark scene."""

from pathlib import Path
import shutil
import sys


sample,circle=map(Path,sys.argv[1:])
addon=circle/"addon/lvgl"
files={}


def edit(path,old,new):
    if path not in files:
        files[path]=path.read_text()
    if files[path].count(old)!=1:
        raise RuntimeError(f"{path.name}: expected one {old!r}")
    files[path]=files[path].replace(old,new)


driver=addon/"lvgl.cpp"
edit(driver,'#include "scroll_present.h"',
     '#include "scroll_present.h"\n#include "lvgl_scroll_reuse.h"')
edit(driver,'#define XDOS_SCROLL_LABEL "vc4"','#define XDOS_SCROLL_LABEL "vc4cache"')
edit(driver,"lv_display_add_event_cb(display,RenderEvent,LV_EVENT_ALL,nullptr);",
     """lv_display_add_event_cb(display,RenderEvent,LV_EVENT_ALL,nullptr);
	lv_display_add_event_cb(display,ScrollBeforeRefresh,LV_EVENT_REFR_START,nullptr);""")
edit(driver,"\t\t\tif (last) RecordPresentation(true);", """			if (scrollScreen) uploadBytes+=w*h*2;
			if (last) {
				ScrollFrameCommitted();
				RecordPresentation(true);
			}""")
edit(sample/"kernel.cpp","XAPP6-PERF7-VC4-EGL / Pi 3 ARM32",
     "XAPP6-PERF7-VC4-Cache / Pi 3 ARM32")
for old,new in (("GEWIDGET.XAPP","GCWIDGET.XAPP"),("GEBENCH.XAPP","GCBENCH.XAPP")):
    edit(sample/"app_shortcut.cpp",old,new)
edit(sample/"xdos_lvgl.cpp",'extern "C" void xdos_scroll_measure_end();',
     'extern "C" void xdos_scroll_measure_end();\nextern "C" void xdos_gpu_scroll_end();')
edit(sample/"xdos_lvgl.cpp","        xdos_scroll_measure_end();",
     "        xdos_gpu_scroll_end();\n        xdos_scroll_measure_end();")
bench=addon/"lvgl/demos/benchmark/lv_demo_benchmark.c"
edit(bench,"extern void xdos_scroll_measure_start(void);",
     """extern void xdos_gpu_scroll_begin(lv_obj_t *);
extern void xdos_gpu_scroll_end(void);
extern int xdos_gpu_scroll_move(lv_obj_t *,int);
extern void xdos_scroll_measure_start(void);""")
edit(bench,"    xdos_scroll_measure_start();",
     "    xdos_gpu_scroll_begin(scr);\n    xdos_scroll_measure_start();")
edit(bench,"    xdos_scroll_measure_end();\n    scene_act++;",
     "    xdos_gpu_scroll_end();\n    xdos_scroll_measure_end();\n    scene_act++;")
edit(bench,"    lv_obj_scroll_to_y(var, v, LV_ANIM_OFF);",
     "    if (!xdos_gpu_scroll_move(var,v)) lv_obj_scroll_to_y(var, v, LV_ANIM_OFF);")

for path,text in files.items():
    path.write_text(text,encoding="utf-8")
shutil.copyfile(Path(__file__).with_name("lvgl_scroll_reuse.h"),addon/"lvgl_scroll_reuse.h")
