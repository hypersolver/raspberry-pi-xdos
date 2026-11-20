#ifndef XDOS_XAPP_ARDUINO_LVGL_H
#define XDOS_XAPP_ARDUINO_LVGL_H

#include "xdos_sdk_v1.h"

typedef void (*XdosSketchRoutine)(void);
typedef struct XdosArduinoLvglApi {
    XdosHostApiV1 base;
    int32_t (*run_sketch)(void *context, XdosSketchRoutine setup,
                          XdosSketchRoutine loop, XdosSketchRoutine stop,
                          int enable_measurements);
} XdosArduinoLvglApi;

#endif
