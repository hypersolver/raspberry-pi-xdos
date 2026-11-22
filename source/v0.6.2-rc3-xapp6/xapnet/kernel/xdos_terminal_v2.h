#pragma once
#include "xdos_sdk_v1.h"
constexpr unsigned XDOS_TERMINAL_V2_CAPABILITY=0x54455232u;
struct XdosTerminalV2Api {
    XdosHostApiV1 base;
    unsigned capability;
    int32_t (*run)(void *,const char *);
    unsigned (*revision)(void *);
    const char *(*text)(void *);
};
