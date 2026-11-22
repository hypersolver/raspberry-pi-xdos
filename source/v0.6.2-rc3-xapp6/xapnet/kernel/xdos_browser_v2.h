#pragma once
#include "xapp_workspace.h"
constexpr unsigned XDOS_BROWSER_V2_CAPABILITY=0x42525732u;
struct XdosBrowserV2Api {
    XdosHostApiV1 base;
    unsigned capability;
    bool (*claim)(bool);
    bool (*run)(void *,const char *,XdosBrowserFetch);
    void (*cancel)(void *);
    bool (*busy)();
    unsigned (*revision)(void *);
    const char *(*text)(void *);
    const char *(*status)(void *);
};
