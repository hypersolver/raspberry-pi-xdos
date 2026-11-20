#ifndef XDOS_XAPP_KEYPAD_H
#define XDOS_XAPP_KEYPAD_H

#include "xdos_sdk_v1.h"

/* The host owns 32 zeroed bytes aligned for uint32_t. Key 0 initializes the
 * display. Title and sixteen keys are copied before OpenKeypad returns. */
typedef void (*XdosKeypadInput)(void *state, int key, char *display);
typedef struct XdosKeypadApi {
    XdosHostApiV1 base;
    int32_t (*open_keypad)(void *context, const char *title, const char *keys,
                           XdosKeypadInput input);
} XdosKeypadApi;

#endif
