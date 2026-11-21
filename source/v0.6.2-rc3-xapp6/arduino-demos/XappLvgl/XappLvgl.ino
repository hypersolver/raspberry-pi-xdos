#include <Arduino.h>
#include <lvgl.h>

static unsigned clicks;

static void onClick(lv_event_t *event)
{
    auto *label = static_cast<lv_obj_t *>(lv_event_get_user_data(event));
    ++clicks;
    lv_label_set_text_fmt(label, "Clicks: %u", clicks);
}

void setup()
{
    clicks = 0;
    auto *button = lv_button_create(lv_screen_active());
    lv_obj_center(button);
    auto *label = lv_label_create(button);
    lv_label_set_text(label, "Clicks: 0");
    lv_obj_center(label);
    lv_obj_add_event_cb(button, onClick, LV_EVENT_CLICKED, label);
}

void loop()
{
    // XDOS handles input, LVGL timers and redraw.
}
