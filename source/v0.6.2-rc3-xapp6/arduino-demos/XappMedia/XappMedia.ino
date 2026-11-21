#include <Arduino.h>
#include <lvgl.h>
#include "photo_rgb565.h"

extern "C" bool xdos_media_play_video(const char *path);
extern "C" bool xdos_media_play_music(const char *path);
extern "C" void xdos_media_stop_music();

static lv_obj_t *status;
static lv_image_dsc_t photo;

static void video(lv_event_t *)
{
    lv_label_set_text(status, xdos_media_play_video("SD:/XDOS/INBOX/DEMO.MP4")
        ? "MP4 queued" : "Cannot open DEMO.MP4");
}

static void music(lv_event_t *)
{
    lv_label_set_text(status, xdos_media_play_music("SD:/XDOS/INBOX/DEMO.WAV")
        ? "Playing music via HDMI" : "Cannot play DEMO.WAV");
}

static void stop(lv_event_t *)
{
    xdos_media_stop_music();
    lv_label_set_text(status, "Music stopped");
}

static void button(lv_obj_t *row, const char *text, lv_event_cb_t clicked)
{
    auto *control = lv_button_create(row);
    auto *caption = lv_label_create(control);
    lv_label_set_text(caption, text);
    lv_obj_center(caption);
    lv_obj_add_event_cb(control, clicked, LV_EVENT_CLICKED, nullptr);
}

void setup()
{
    auto *screen = lv_screen_active();
    photo.header.magic = LV_IMAGE_HEADER_MAGIC;
    photo.header.cf = LV_COLOR_FORMAT_RGB565;
    photo.header.w = PHOTO_WIDTH;
    photo.header.h = PHOTO_HEIGHT;
    photo.header.stride = PHOTO_WIDTH * 2;
    photo.data_size = sizeof photo_pixels;
    photo.data = photo_pixels;
    auto *image = lv_image_create(screen);
    lv_image_set_src(image, &photo);
    lv_obj_align(image, LV_ALIGN_CENTER, 0, -54);

    auto *row = lv_obj_create(screen);
    lv_obj_set_size(row, 365, 60);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 66);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    button(row, "Play MP4", video);
    button(row, "Play WAV", music);
    button(row, "Stop", stop);

    status = lv_label_create(screen);
    lv_label_set_text(status, "Media example");
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 115);
}

void loop()
{
}
