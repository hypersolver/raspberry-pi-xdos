#include <Arduino.h>  // Arduino core types and timing functions. / Arduino 核心型別與計時函式。
#include <lvgl.h>  // LVGL widgets and rendering API supplied by XDOS. / XDOS 提供的 LVGL 元件和繪圖介面。

static unsigned clicks;

static void onClick(lv_event_t *event)
{
    auto *label = static_cast<lv_obj_t *>(lv_event_get_user_data(event));  // Get the label attached to this click callback. / 取得傳給點擊回呼的文字標籤。
    ++clicks;  // Count this button click. / 將本次按鈕點擊計入。
    lv_label_set_text_fmt(label, "Clicks: %u", clicks);  // Update the label with the new total. / 將最新計數顯示在標籤上。
}

void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    clicks = 0;  // Reset the count each time the XAPP starts. / 每次啟動 XAPP 時將計數歸零。
    auto *button = lv_button_create(lv_screen_active());  // Place a button on the active screen. / 在目前畫面放置按鈕。
    lv_obj_center(button);  // Center the button in the window. / 將按鈕在視窗中置中。
    auto *label = lv_label_create(button);  // Make the label a child of the button. / 在按鈕下建立子標籤。
    lv_label_set_text(label, "Clicks: 0");  // Display the initial click count. / 顯示初始點擊次數。
    lv_obj_center(label);  // Center the caption within its button. / 將按鈕內的文字置中。
    lv_obj_add_event_cb(button, onClick, LV_EVENT_CLICKED, label);  // Call onClick and pass the label when clicked. / 點擊時呼叫 onClick 並傳入標籤。
}

void loop()  // Run repeatedly while the sketch is active. / 草稿執行期間重複呼叫。
{
    // XDOS handles input, LVGL timers and redraw. / XDOS 處理輸入、LVGL 計時器與重繪。
}
