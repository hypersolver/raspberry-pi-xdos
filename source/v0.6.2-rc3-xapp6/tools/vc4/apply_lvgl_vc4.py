"""Install the experimental GLES2 display backend on top of VSyncOpt."""

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


edit(circle/"addon/linux/semaphore.cpp",
     "\t\tCScheduler::Get ()->Yield ();",
     """		// VCHIQ workers wait here even when the desktop is idle.
		// Yield keeps the waiter runnable and prevents scheduler WFI.
		CScheduler::Get ()->MsSleep (1);""")

edit(circle/"addon/linux/completion.cpp",
     """	while (x->done == 0)
	{
		CScheduler::Get ()->Yield ();
	}""",
     """	while (x->done == 0)
	{
		// The VCHIQ keep-alive worker waits here while the desktop is idle.
		CScheduler::Get ()->MsSleep (1);
	}""")

edit(sample/"Makefile","OBJS += xdos_lvgl.o xdos_benchmark.o",
     "OBJS += xdos_lvgl.o xdos_benchmark.o lvgl_vc4.o")
edit(sample/"Makefile","LIBS := $(CIRCLEHOME)/addon/lvgl/liblvgl.a $(LIBS)",
     """LIBS := $(CIRCLEHOME)/addon/lvgl/liblvgl.a \
    $(CIRCLEHOME)/addon/vc4/interface/bcm_host/libbcm_host.a \
    $(CIRCLEHOME)/addon/vc4/interface/khronos/libkhrn_client.a \
    $(CIRCLEHOME)/addon/vc4/interface/vmcs_host/libvmcs_host.a \
    $(CIRCLEHOME)/addon/vc4/interface/vcos/libvcos.a \
    $(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
    $(CIRCLEHOME)/addon/linux/liblinuxemu.a $(LIBS)""")
kernel=sample/"kernel.cpp"
edit(kernel,'#include "p0_metrics.h"',"""#include "p0_metrics.h"
#include "lvgl_vc4.h"
#include <circle/memory.h>
#include <vc4/vchiq/vchiqdevice.h>
extern "C" unsigned xdos_vc4_clock_us() { return CTimer::GetClockTicks(); }""")
edit(kernel,"XAPP6-PERF7-VSyncNeon / Pi 3 ARM32","XAPP6-PERF7-VC4-EGL / Pi 3 ARM32")
edit(kernel,"    lv_refr_now(lv_display_get_default());", """    static CVCHIQDevice gpu(CMemorySystem::Get(),&interrupt_);
    if (gpu.Initialize() && xdos_vc4_start(screenWidth_,screenHeight_))
        Debug("[VC4 LVGL] GLES2 presentation active; EGL swap interval=1\\r\\n");
    else
        Debug("[VC4 LVGL] VSyncOpt fallback\\r\\n");
    lv_refr_now(lv_display_get_default());""")
for old,new in (("NEWIDGET.XAPP","GEWIDGET.XAPP"),("NEBENCH.XAPP","GEBENCH.XAPP")):
    edit(sample/"app_shortcut.cpp",old,new)
driver=addon/"lvgl.cpp"
edit(driver,'#define XDOS_SCROLL_LABEL "neon"',
     '#define XDOS_SCROLL_LABEL "vc4"\n#define XDOS_SCROLL_GPU 1\n#include "lvgl_vc4.h"')
edit(driver,"static volatile bool xdos_last_chunk = false;",
     """static volatile bool xdos_last_chunk = false;
static bool vc4_redraw_after_failure = false;""")
edit(driver,"\t\txdos_present_frame();", """		xdos_present_frame();
		if (vc4_redraw_after_failure) {
			vc4_redraw_after_failure = false;
			lv_obj_invalidate(lv_screen_active());
		}""")
edit(driver,"\tconst bool last = lv_display_flush_is_last(pDisplay);",
     """	const bool last = lv_display_flush_is_last(pDisplay);
	if (s_pThis->m_pScreen && s_pThis->m_pDisplay->GetDepth()==16) {
		XdosVc4Timing timing;
		const int result=xdos_vc4_upload(Area.x1,Area.y1,w,h,pBuffer,last,&timing);
		if (result>0) {
			RecordGpuChunk(timing,last);
			if (last) RecordPresentation(true);
			lv_display_flush_ready(pDisplay);
			return;
		}
		if (result<0) {
			xdos_vc4_stop();
			vc4_redraw_after_failure = true;
			CLogger::Get()->Write("VC4 LVGL",LogWarning,"GPU flush failed; switching to VSyncOpt");
		}
	}""")
for path,text in files.items():
    path.write_text(text,encoding="utf-8")
for name in ("lvgl_vc4.c","lvgl_vc4.h"):
    shutil.copyfile(Path(__file__).with_name(name),sample/name)
shutil.copyfile(Path(__file__).with_name("lvgl_vc4.h"),addon/"lvgl_vc4.h")
