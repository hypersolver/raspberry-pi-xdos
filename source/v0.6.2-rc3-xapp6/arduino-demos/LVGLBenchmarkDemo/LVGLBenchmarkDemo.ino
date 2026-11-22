#include <Arduino.h>  // Arduino core types and timing functions. / Arduino 核心型別與計時函式。
#include <lvgl.h>  // LVGL widgets and rendering API supplied by XDOS. / XDOS 提供的 LVGL 元件和繪圖介面。
#include <demos/lv_demos.h>  // Declarations for the official LVGL demos. / 官方 LVGL 示範的函式宣告。

#if !LV_USE_DEMO_BENCHMARK || !LV_USE_SYSMON || !LV_USE_PERF_MONITOR
#error "Enable LV_USE_DEMO_BENCHMARK, LV_USE_SYSMON and LV_USE_PERF_MONITOR."
#endif

static bool ready;

void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    // The RPiduino display port must initialize LVGL and its display first. / RPiDuino 顯示移植層須先初始化 LVGL 與顯示器。
    if (!lv_display_get_default()) return;  // Skip setup if the host has not initialized a display. / 若主系統尚未初始化顯示器，就結束本次設定。
    lv_demo_benchmark();  // Start the official rendering benchmark. / 啟動官方繪圖效能測試。
    ready = true;  // Mark the demo as initialized. / 標記示範已完成初始化。
}

void loop()  // Run repeatedly while the sketch is active. / 草稿執行期間重複呼叫。
{
#if !defined(RPIDUINO_LVGL_HOST_TIMER)
    if (ready) lv_timer_handler();  // Advance LVGL timers when XDOS is not driving them. / 當 XDOS 未接管時，處理 LVGL 計時器。
    delay(5);  // Pause briefly between timer updates. / 兩次計時更新之間短暫等待。
#endif
}
