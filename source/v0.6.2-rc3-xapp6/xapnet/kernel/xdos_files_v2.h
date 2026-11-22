#ifndef XDOS_FILES_V2_H
#define XDOS_FILES_V2_H

#include "xdos_sdk_v1.h"
#include "xdos_directory.h"

struct _lv_obj_t;

constexpr unsigned XDOS_FILES_V2_CAPABILITY=0x46494c32u;

struct XdosFilesV2Api {
    XdosHostApiV1 base;
    unsigned capability;
    decltype(&xdos_directory_visit) visit;
    int (*preview)(const char *, char *, unsigned, unsigned *, int *);
    int (*remove)(const char *);
    int (*launch)(const char *);
    bool (*claim)();
    void (*setPanes)(_lv_obj_t *, _lv_obj_t *);
};

bool XdosFilesClaim();
void XdosFilesSetPanes(_lv_obj_t *, _lv_obj_t *);
int XdosFilesPreview(const char *, char *, unsigned, unsigned *, int *);
int XdosFilesQueueLaunch(const char *);
const char *XdosFilesTakeLaunch();

#endif
