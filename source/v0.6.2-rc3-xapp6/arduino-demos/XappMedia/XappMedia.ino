#include <Arduino.h>  // Arduino core types and timing functions. / Arduino 核心型別與計時函式。
#include <lvgl.h>  // LVGL widgets and rendering API supplied by XDOS. / XDOS 提供的 LVGL 元件和繪圖介面。
#include "photo_rgb565.h"  // Generated RGB565 pixels for the sample photo. / 範例照片轉換後的 RGB565 像素。

extern "C" bool xdos_media_play_video(const char *path);  // Ask XDOS to queue an MP4 file. / 請 XDOS 排入 MP4 播放。
extern "C" bool xdos_media_play_music(const char *path);  // Ask XDOS to start audio playback. / 請 XDOS 開始播放音訊。
extern "C" void xdos_media_stop_music();  // Access the host audio stop function. / 使用主系統的音訊停止函式。

static lv_obj_t *status;  // Keep the status label available to callbacks. / 保留狀態標籤供回呼使用。
static lv_image_dsc_t photo;  // Keep the image descriptor alive while LVGL uses it. / 在 LVGL 使用期間保留圖片描述結構。

static void video(lv_event_t *)  // Handle clicks on Play MP4. / 處理播放 MP4 的點擊。
{
    lv_label_set_text(status, xdos_media_play_video("SD:/XDOS/INBOX/DEMO.MP4")
        ? "MP4 queued" : "Cannot open DEMO.MP4");
}

static void music(lv_event_t *)  // Handle clicks on Play WAV. / 處理播放 WAV 的點擊。
{
    lv_label_set_text(status, xdos_media_play_music("SD:/XDOS/INBOX/DEMO.WAV")
        ? "Playing music via HDMI" : "Cannot play DEMO.WAV");
}

static void stop(lv_event_t *)  // Handle clicks on Stop. / 處理停止按鈕的點擊。
{
    xdos_media_stop_music();  // Stop audio playback through the host service. / 透過主系統服務停止音訊播放。
    lv_label_set_text(status, "Music stopped");  // Show that the audio stop command ran. / 顯示已執行音訊停止指令。
}

static void button(lv_obj_t *row, const char *text, lv_event_cb_t clicked)  // Create a labeled control with a click callback. / 建立附標籤與點擊回呼的控制項。
{
    auto *control = lv_button_create(row);  // Add the button to the controls row. / 在控制列加入按鈕。
    auto *caption = lv_label_create(control);  // Create a label inside the button. / 在按鈕內建立標籤。
    lv_label_set_text(caption, text);  // Set the button caption. / 設定按鈕文字。
    lv_obj_center(caption);  // Center the caption within the button. / 將文字置於按鈕中央。
    lv_obj_add_event_cb(control, clicked, LV_EVENT_CLICKED, nullptr);  // Call the supplied handler on a click. / 點擊時呼叫指定的處理函式。
}

void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    auto *screen = lv_screen_active();  // Use the active screen as the parent for the UI. / 使用目前畫面作為介面的父物件。
    photo.header.magic = LV_IMAGE_HEADER_MAGIC;  // Mark this as a valid LVGL image header. / 標示為有效的 LVGL 圖片標頭。
    photo.header.cf = LV_COLOR_FORMAT_RGB565;  // Declare the format of the bundled pixels. / 宣告內附像素的色彩格式。
    photo.header.w = PHOTO_WIDTH;  // Set the photo width in pixels. / 設定照片寬度，單位為像素。
    photo.header.h = PHOTO_HEIGHT;  // Set the photo height in pixels. / 設定照片高度，單位為像素。
    photo.header.stride = PHOTO_WIDTH * 2;  // Set row length to two bytes per RGB565 pixel. / 依每個 RGB565 像素兩個位元組設定列長。
    photo.data_size = sizeof photo_pixels;  // Record the image buffer size. / 記錄圖片緩衝區大小。
    photo.data = photo_pixels;  // Point the image descriptor at its bundled pixels. / 讓圖片描述結構指向內附像素。
    auto *image = lv_image_create(screen);  // Create the image widget on the screen. / 在畫面建立圖片元件。
    lv_image_set_src(image, &photo);  // Display the bundled RGB565 photo. / 顯示內附的 RGB565 照片。
    lv_obj_align(image, LV_ALIGN_CENTER, 0, -54);  // Place the photo above the controls. / 將照片放在控制列上方。

    auto *row = lv_obj_create(screen);  // Create a container for the playback controls. / 建立播放控制項的容器。
    lv_obj_set_size(row, 365, 60);  // Set the control row dimensions. / 設定控制列的寬度和高度。
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 66);  // Place the controls below the photo. / 將控制項放在照片下方。
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);  // Place the media controls side by side. / 將影音控制項並排擺放。
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // Space the buttons evenly and center them vertically. / 平均分配按鈕間距並垂直置中。
    button(row, "Play MP4", video);  // Connect this button to the video callback. / 將按鈕連接到影片播放回呼。
    button(row, "Play WAV", music);  // Connect this button to the HDMI audio callback. / 將按鈕連接到 HDMI 音訊回呼。
    button(row, "Stop", stop);  // Connect this button to the audio stop callback. / 將按鈕連接到停止音訊的回呼。

    status = lv_label_create(screen);  // Create the playback status label. / 建立播放狀態標籤。
    lv_label_set_text(status, "Media example");  // Show the initial status before playback. / 在播放前顯示初始狀態。
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 115);  // Position the status below the controls. / 將狀態放在控制項下方。
}

void loop()  // Run repeatedly while the sketch is active. / 草稿執行期間重複呼叫。
{
}
