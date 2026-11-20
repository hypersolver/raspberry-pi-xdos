#include <Arduino.h>
#include <lvgl.h>
#include <demos/lv_demos.h>

#if !LV_USE_DEMO_BENCHMARK || !LV_USE_SYSMON || !LV_USE_PERF_MONITOR
#error "Enable LV_USE_DEMO_BENCHMARK, LV_USE_SYSMON and LV_USE_PERF_MONITOR."
#endif

static bool ready;

void setup()
{
    // The RPiduino display port must initialize LVGL and its display first.
    if (!lv_display_get_default()) return;
    lv_demo_benchmark();
    ready = true;
}

void loop()
{
#if !defined(RPIDUINO_LVGL_HOST_TIMER)
    if (ready) lv_timer_handler();
    delay(5);
#endif
}
