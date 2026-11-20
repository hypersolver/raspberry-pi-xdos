#include <Arduino.h>
#include <lvgl.h>
#include <demos/lv_demos.h>

#if !LV_USE_DEMO_WIDGETS
#error "Enable LV_USE_DEMO_WIDGETS in the sketch's LVGL configuration."
#endif

static bool ready;
#if defined(XDOS_WIDGETS_ANALYTICS_TEST)
extern "C" void xdos_widgets_analytics_test(void);
#endif

void setup()
{
    // The RPiduino display port must initialize LVGL and its display first.
    if (!lv_display_get_default()) return;
    lv_demo_widgets();
#if defined(XDOS_WIDGETS_ANALYTICS_TEST)
    xdos_widgets_analytics_test();
#endif
    ready = true;
}

void loop()
{
#if !defined(RPIDUINO_LVGL_HOST_TIMER)
    if (ready) lv_timer_handler();
    delay(5);
#endif
}
