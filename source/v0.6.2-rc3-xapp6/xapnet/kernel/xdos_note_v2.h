#ifndef XDOS_NOTE_V2_H
#define XDOS_NOTE_V2_H

#include "xdos_sdk_v1.h"

#define XDOS_NOTE_V2_CAPABILITY 0x4e4f5432u
#define XDOS_NOTE_MAX_BYTES 2047u

typedef struct {
    unsigned storage_revision, note_revision;
    const char *text;
    const char *status;
} XdosNoteSnapshot;

typedef struct {
    XdosHostApiV1 base;
    unsigned capability;
    int (*load)(void *context);
    int (*save)(void *context, const char *text);
    void (*snapshot)(void *context, XdosNoteSnapshot *out);
} XdosNoteV2Api;

#endif
