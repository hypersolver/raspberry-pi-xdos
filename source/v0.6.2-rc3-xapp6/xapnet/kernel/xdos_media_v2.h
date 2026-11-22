#pragma once
#include "xdos_files_v2.h"
constexpr unsigned XDOS_MEDIA_V2_CAPABILITY=0x4d454432u;
struct XdosMediaV2Api {
    XdosHostApiV1 base;
    unsigned capability;
    decltype(&xdos_directory_visit) visit;
    int (*play)(const char *);
    int (*jack)(const char *);
    void (*stop)();
};
