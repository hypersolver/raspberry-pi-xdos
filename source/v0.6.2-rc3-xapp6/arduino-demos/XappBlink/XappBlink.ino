#include <Arduino.h>  // Arduino core types and timing functions. / Arduino 核心型別與計時函式。
#include <lvgl.h>  // LVGL widgets and rendering API supplied by XDOS. / XDOS 提供的 LVGL 元件和繪圖介面。

extern "C" void xdos_arduino_set_act_led(int on);

static lv_obj_t *status;
static unsigned long lastChange;
static bool lit;

static void show(bool on)
{
    lit = on;  // Remember the LED state so the next tick can invert it. / 記住 LED 狀態，供下次切換。
    xdos_arduino_set_act_led(on);  // Set the Pi ACT LED through the XDOS host. / 透過 XDOS 控制 Pi 的 ACT LED。
    lv_label_set_text(status, on ? "ACT LED: ON" : "ACT LED: OFF");  // Show the current LED state on the screen. / 在畫面顯示目前 LED 狀態。
}

void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    status = lv_label_create(lv_screen_active());  // Create a label on the active LVGL screen. / 在目前的 LVGL 畫面建立標籤。
    lv_obj_center(status);  // Center the status label on the screen. / 將狀態文字置中。
    lastChange = millis();  // Record the time of the initial LED update. / 記錄第一次更新 LED 的時間。
    show(true);  // Begin with the ACT LED lit. / 啟動時先點亮 ACT LED。
}

void loop()  // Run repeatedly while the sketch is active. / 草稿執行期間重複呼叫。
{
    const unsigned long now = millis();  // Read elapsed milliseconds without blocking XDOS. / 讀取經過的毫秒數，讓 XDOS 繼續執行。
    if (now - lastChange >= 500) {  // Toggle after 500 ms. Unsigned subtraction handles wraparound. / 每 500 毫秒切換，無號減法可處理計時器回繞。
        lastChange = now;  // Start the next half-second interval. / 開始下一個半秒計時。
        show(!lit);  // Toggle the physical LED and its on-screen label. / 切換實體 LED 並更新畫面文字。
    }
}

extern "C" void xdos_stop_blink()
{
    xdos_arduino_set_act_led(false);  // Turn the ACT LED off when the XAPP closes. / 關閉 XAPP 時熄滅 ACT LED。
}

extern "C" void xdos_sketch_stop() { xdos_stop_blink(); }  // Expose cleanup to the Arduino XAPP launcher. / 讓 Arduino XAPP 啟動器可呼叫清理函式。
