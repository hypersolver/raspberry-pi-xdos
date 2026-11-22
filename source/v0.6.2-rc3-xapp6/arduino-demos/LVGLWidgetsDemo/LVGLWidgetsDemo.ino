#include <Arduino.h>  // Arduino core types and timing functions. / Arduino 核心型別與計時函式。
#include <lvgl.h>  // LVGL widgets and rendering API supplied by XDOS. / XDOS 提供的 LVGL 元件和繪圖介面。
#include <demos/lv_demos.h>  // Declarations for the official LVGL demos. / 官方 LVGL 示範的函式宣告。

#if !LV_USE_DEMO_WIDGETS
#error "Enable LV_USE_DEMO_WIDGETS in the sketch's LVGL configuration."
#endif

static bool ready;
#if defined(XDOS_WIDGETS_ANALYTICS_TEST)
extern "C" void xdos_widgets_analytics_test(void);
#endif

void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    // The RPiduino display port must initialize LVGL and its display first. / RPiDuino 顯示移植層須先初始化 LVGL 與顯示器。
    if (!lv_display_get_default()) return;  // Skip setup if the host has not initialized a display. / 若主系統尚未初始化顯示器，就結束本次設定。
    lv_demo_widgets();  // Start the official widget gallery. / 啟動官方元件示範。
#if defined(XDOS_WIDGETS_ANALYTICS_TEST)
    xdos_widgets_analytics_test();  // Start the optional XDOS analytics test. / 啟動選配的 XDOS Analytics 測試。
#endif
    ready = true;  // Mark the demo as initialized. / 標記示範已完成初始化。
}

void loop()  // Run repeatedly while the sketch is active. / 草稿執行期間重複呼叫。
{
#if !defined(RPIDUINO_LVGL_HOST_TIMER)
    if (ready) lv_timer_handler();  // Advance LVGL timers when XDOS is not driving them. / 當 XDOS 未接管時，處理 LVGL 計時器。
    delay(5);  // Pause briefly between timer updates. / 兩次計時更新之間短暫等待。
#endif
}
