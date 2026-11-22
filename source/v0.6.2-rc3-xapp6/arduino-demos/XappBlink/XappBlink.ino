#include <Arduino.h>
#include <lvgl.h>

extern "C" void xdos_arduino_set_act_led(int on);

static lv_obj_t *status;
static unsigned long lastChange;
static bool lit;

static void show(bool on)
{
    lit = on;
    xdos_arduino_set_act_led(on);
    lv_label_set_text(status, on ? "ACT LED: ON" : "ACT LED: OFF");
}

void setup()
{
    status = lv_label_create(lv_screen_active());
    lv_obj_center(status);
    lastChange = millis();
    show(true);
}

void loop()
{
    const unsigned long now = millis();
    if (now - lastChange >= 500) {
        lastChange = now;
        show(!lit);
    }
}

extern "C" void xdos_stop_blink()
{
    xdos_arduino_set_act_led(false);
}

extern "C" void xdos_sketch_stop() { xdos_stop_blink(); }
