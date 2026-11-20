"""Build a matched Widgets Analytics test with opt-in container texture reuse."""

from pathlib import Path
import shutil
import sys


sample,circle=map(Path,sys.argv[1:3])
mode=sys.argv[3]
label,prefix={
    "analyticsbase": ("AnalyticsBase2","A2"),
    "analyticscache": ("AnalyticsSafe","CS"),
    "analyticsregion": ("AnalyticsRegion","RG"),
}[mode]
addon=circle/"addon/lvgl"
files={}


def edit(path,old,new):
    if path not in files:
        files[path]=path.read_text()
    if files[path].count(old)!=1:
        raise RuntimeError(f"{path.name}: expected one {old!r}")
    files[path]=files[path].replace(old,new)


driver=addon/"lvgl.cpp"
edit(driver,'#include "lvgl_scroll_reuse.h"',
     f'#include "lvgl_scroll_reuse.h"\n#define XDOS_ANALYTICS_CACHE {int(mode!="analyticsbase")}\n'
     f'#define XDOS_ANALYTICS_REGION {int(mode=="analyticsregion")}\n'
     '#include "lvgl_container_reuse.h"')
edit(driver,'#define XDOS_SCROLL_LABEL "vc4cache"',f'#define XDOS_SCROLL_LABEL "{mode}"')
edit(driver,"if (scrollScreen) uploadBytes+=w*h*2;",
     "if (scrollScreen || cachedContainer) uploadBytes+=w*h*2;")
edit(driver,"\t\t\t\tScrollFrameCommitted();",
     "\t\t\t\tScrollFrameCommitted();\n\t\t\t\tContainerFrameCommitted();")
edit(sample/"kernel.cpp","XAPP6-PERF7-VC4-Cache / Pi 3 ARM32",f"XAPP6-PERF7-VC4-{label} / Pi 3 ARM32")
for ending in ("WIDGET","BENCH"):
    edit(sample/"app_shortcut.cpp",f"GC{ending}.XAPP",f"{prefix}{ending}.XAPP")
core=addon/"lvgl/src/core/lv_obj_scroll.c"
edit(core,"lv_result_t lv_obj_scroll_by_raw(lv_obj_t * obj, int32_t x, int32_t y)\n{",
     """extern bool xdos_container_scroll_damage(lv_obj_t *,int,int);

lv_result_t lv_obj_scroll_by_raw(lv_obj_t * obj, int32_t x, int32_t y)
{""")
edit(core,"if(res != LV_RESULT_OK) return res;\n    lv_obj_invalidate(obj);\n    return LV_RESULT_OK;",
     """if(res != LV_RESULT_OK) return res;
    if (!xdos_container_scroll_damage(obj,x,y)) lv_obj_invalidate(obj);
    return LV_RESULT_OK;""")
refresh=addon/"lvgl/src/core/lv_refr.c"
edit(refresh,"static void draw_buf_flush(lv_display_t * disp);",
     "extern void xdos_container_prepare_refresh(lv_display_t *);\n"
     "static void draw_buf_flush(lv_display_t * disp);")
edit(refresh,"    lv_refr_join_area();\n    refr_sync_areas();",
     "    xdos_container_prepare_refresh(disp_refr);\n    lv_refr_join_area();\n    refr_sync_areas();")

widgets=addon/"lvgl/demos/widgets/lv_demo_widgets.c"
edit(widgets,"void lv_demo_widgets(void)\n{", """extern void xdos_analytics_start(lv_obj_t *);
extern void xdos_analytics_end(void);
extern lv_result_t lv_obj_scroll_by_raw(lv_obj_t *,int32_t,int32_t);
static lv_timer_t *analytics_test_timer;
static uint32_t analytics_started;
static int32_t analytics_distance;

static void analytics_test_deleted(lv_event_t *event)
{
    LV_UNUSED(event);
    if (analytics_test_timer) lv_timer_delete(analytics_test_timer);
    analytics_test_timer=NULL;
    xdos_analytics_end();
}

static void analytics_test_tick(lv_timer_t *timer)
{
    lv_obj_t *tab=lv_timer_get_user_data(timer);
    if (!analytics_started) {
        lv_obj_update_layout(tab);
        analytics_distance=lv_obj_get_scroll_bottom(tab);
        if (analytics_distance<=0) {
            lv_timer_delete(timer);
            analytics_test_timer=NULL;
            return;
        }
        if (lv_obj_send_event(tab,LV_EVENT_SCROLL_BEGIN,NULL)!=LV_RESULT_OK) return;
        analytics_started=lv_tick_get();
        xdos_analytics_start(tab);
        lv_timer_set_period(timer,20);
    }
    const uint32_t elapsed=lv_tick_elaps(analytics_started);
    if (elapsed>=5200) {
        lv_timer_delete(timer);
        analytics_test_timer=NULL;
        xdos_analytics_end();
        lv_obj_send_event(tab,LV_EVENT_SCROLL_END,NULL);
        return;
    }
    const uint32_t period=elapsed%3200;
    const uint32_t step=period<1600 ? period : 3200-period;
    const int32_t target=analytics_distance*(int32_t)step/1600;
    lv_obj_scroll_by_raw(tab,0,lv_obj_get_scroll_y(tab)-target);
}

void lv_demo_widgets(void)
{""")
# Only auto-run for the direct Widgets XAPP, not the Widgets scene inside Benchmark.
edit(widgets,"void lv_demo_widgets_start_slideshow(void)", """void xdos_widgets_analytics_test(void)
{
    lv_tabview_set_active(tv,1,LV_ANIM_OFF);
    lv_obj_t *tab=lv_obj_get_child(lv_tabview_get_content(tv),1);
    analytics_started=0;
    analytics_test_timer=lv_timer_create(analytics_test_tick,600,tab);
    lv_obj_add_event_cb(tab,analytics_test_deleted,LV_EVENT_DELETE,NULL);
}

void lv_demo_widgets_start_slideshow(void)""")
for path,text in files.items():
    path.write_text(text,encoding="utf-8")
shutil.copyfile(Path(__file__).with_name("lvgl_container_reuse.h"),addon/"lvgl_container_reuse.h")
