"""Instrument the official scrolling scene and optionally present via two scanout pages."""

from pathlib import Path
import shutil
import sys


source, circle = map(Path,sys.argv[1:3])
mode = sys.argv[3]
if mode not in {"baseline","vsync","optimized","reference","neon"}:
    raise ValueError("Use baseline, vsync, optimized, reference or neon")
vsync = mode != "baseline"
optimized = mode in {"optimized","reference","neon"}
render_stats = mode in {"reference","neon"}
addon = circle / "addon/lvgl"
driver = addon / "lvgl.cpp"
bench = addon / "lvgl/demos/benchmark/lv_demo_benchmark.c"
files = {}


def edit(path, old, new):
    if path not in files:
        files[path] = path.read_text()
    if files[path].count(old) != 1:
        raise RuntimeError(f"{path.name}: expected one occurrence of {old!r}")
    files[path] = files[path].replace(old,new)


# Circle's P0 and refresh patches are applied later in a clean build. Insert
# our hook after those patches, using their exact anchors.
edit(driver, "CLVGL *CLVGL::s_pThis = 0;",
     f'#define XDOS_SCROLL_VSYNC {int(vsync)}\n'
     f'#define XDOS_SCROLL_OPTIMIZED {int(optimized)}\n'
     f'#define XDOS_SCROLL_RENDER_STATS {int(render_stats)}\n'
     f'#define XDOS_SCROLL_LABEL "{mode}"\n#include "scroll_present.h"\n'
     "static volatile bool xdos_last_chunk = false;\n\nCLVGL *CLVGL::s_pThis = 0;")
edit(driver, "\tCTimer::Get ()->RegisterPeriodicHandler (PeriodicTickHandler);",
     """#if XDOS_SCROLL_VSYNC
	if (m_pScreen) InitPages(m_pScreen->GetFrameBuffer());
#endif
#if XDOS_SCROLL_RENDER_STATS
	lv_display_add_event_cb(display,RenderEvent,LV_EVENT_ALL,nullptr);
#endif
	CTimer::Get ()->RegisterPeriodicHandler (PeriodicTickHandler);""")
edit(driver, "\t\tunsigned wait_ms = lv_timer_handler ();",
     """		unsigned wait_ms = lv_timer_handler ();
		xdos_present_frame();""")
edit(driver, "\txdos_p0_dirty_pixels += pixels;", """	xdos_p0_dirty_pixels += pixels;
	const bool last = lv_display_flush_is_last(pDisplay);
#if XDOS_SCROLL_VSYNC
	BeginChunk(Area.x1,Area.x2+1,Area.y1,Area.y2+1);
	Area.y1 += drawY;
	Area.y2 += drawY;
#endif""")
edit(driver, """			    16, fb->GetWidth (), fb->GetHeight (), pArea->x1, pArea->y1,
			    pArea->x2, pArea->y2, (const u16 *)pBuffer);
			++xdos_p0_cpu_flushes;
			lv_display_flush_ready (pDisplay);""",
     """			    16, fb->GetWidth (), fb->GetSize()/fb->GetPitch(), Area.x1, Area.y1,
			    Area.x2, Area.y2, (const u16 *)pBuffer);
			++xdos_p0_cpu_flushes;
			FinishChunk(last);
			lv_display_flush_ready (pDisplay);""")
edit(driver, "\ts_pThis->m_pDisplay->SetArea (Area, pBuffer, DisplayFlushComplete, pDisplay);",
     """	xdos_last_chunk = last;
#if XDOS_SCROLL_VSYNC
	if (last && pages && drawY!=visibleY) waitingFinalDma = true;
#endif
	s_pThis->m_pDisplay->SetArea (Area, pBuffer, DisplayFlushComplete, pDisplay);""")
edit(driver, "\t\t++xdos_p0_dma_completed;", """		++xdos_p0_dma_completed;
		if (measuringScroll) dmaTimes.Add(elapsed,25);""")
edit(driver, "\tassert (pDisplay != 0);\n\n\tlv_display_flush_ready (pDisplay);",
     "\tassert (pDisplay != 0);\n\tFinishChunk(xdos_last_chunk);\n\n\tlv_display_flush_ready (pDisplay);")
if vsync:
    edit(circle / "lib/screen.cpp", "0, 0, m_nDisplay);", "0, 0, m_nDisplay, TRUE);")

edit(bench, "static void containers_with_scrolling_cb(void)\n{",
     """extern void xdos_scroll_measure_start(void);
extern void xdos_scroll_measure_end(void);

static void containers_with_scrolling_cb(void)
{""")
edit(bench, """    scroll_anim(scr, lv_obj_get_scroll_bottom(scr));
}

static void widgets_demo_cb(void)""", """    scroll_anim(scr, lv_obj_get_scroll_bottom(scr));
    xdos_scroll_measure_start();
}

static void widgets_demo_cb(void)""")
edit(bench, "    scene_act++;\n\n    load_scene(scene_act);",
     "    xdos_scroll_measure_end();\n    scene_act++;\n\n    load_scene(scene_act);")
edit(source / "kernel.cpp", '#include "p0_metrics.h"',
     '#include "p0_metrics.h"\nextern "C" void xdos_present_frame();')
edit(source / "kernel.cpp", "    LogBootTiming(xdos_p0_dma_submitted == xdos_p0_dma_completed ?",
     "    xdos_present_frame();\n    LogBootTiming(xdos_p0_dma_submitted == xdos_p0_dma_completed ?")
label,prefix = {
    "baseline": ("Baseline","BL"), "vsync": ("VSync","VS"), "optimized": ("VSyncOpt","VO"),
    "reference": ("VSyncRef","NR"), "neon": ("VSyncNeon","NE"),
}[mode]
if mode == "neon":
    edit(addon / "lv_conf.h", "LV_USE_DRAW_SW_ASM     LV_DRAW_SW_ASM_NONE",
         "LV_USE_DRAW_SW_ASM     LV_DRAW_SW_ASM_NEON")
edit(source / "kernel.cpp", "XAPP6-PERF7 / Pi 3 ARM32",f"XAPP6-PERF7-{label} / Pi 3 ARM32")
for name in ("WIDGET","BENCH"):
    edit(source / "app_shortcut.cpp", f"P7{name}.XAPP",f"{prefix}{name}.XAPP")
edit(source / "xdos_lvgl.cpp", "void XDOSLVGL::DemoBack(lv_event_t *event)",
     'extern "C" void xdos_scroll_measure_end();\n\nvoid XDOSLVGL::DemoBack(lv_event_t *event)')
edit(source / "xdos_lvgl.cpp", "    if (ui->sketchActive_) {",
     "    if (ui->sketchActive_) {\n        xdos_scroll_measure_end();")

for path, text in files.items():
    path.write_text(text,encoding="utf-8")
shutil.copyfile(Path(__file__).with_name("scroll_present.h"),addon / "scroll_present.h")
