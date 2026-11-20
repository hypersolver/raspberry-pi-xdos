#ifndef XDOS_XAPP_EDITOR_H
#define XDOS_XAPP_EDITOR_H

#include "xapp_keypad.h"

enum { XDOS_EDITOR_LOAD = 1, XDOS_EDITOR_SAVE = 2 };
typedef int32_t (*XdosNoteRequest)(void *context, int action, const char *text);
typedef int32_t (*XdosEditorAction)(XdosNoteRequest request, void *context,
                                   int action, const char *text);

typedef struct XdosEditorApi {
    XdosKeypadApi keypad;
    int32_t (*open_editor)(void *context, const char *title, XdosEditorAction action);
} XdosEditorApi;

#endif
