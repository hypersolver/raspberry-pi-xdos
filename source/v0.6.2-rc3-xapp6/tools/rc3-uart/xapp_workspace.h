#ifndef XDOS_XAPP_WORKSPACE_H
#define XDOS_XAPP_WORKSPACE_H

#include "xapp_editor.h"

enum {
    XDOS_PANEL_FILES = 1, XDOS_PANEL_HTTP = 2, XDOS_PANEL_UPDATE = 3, XDOS_PANEL_TERMINAL = 4,
    XDOS_REQUEST_LIST = 1, XDOS_REQUEST_LIST_USB, XDOS_REQUEST_READ,
    XDOS_REQUEST_HTTP, XDOS_REQUEST_CANCEL_HTTP,
    XDOS_REQUEST_SELECT_IMAGE, XDOS_REQUEST_RESTORE_BOOT, XDOS_REQUEST_RESET,
    XDOS_REQUEST_TERMINAL
};
enum { XDOS_PAINT_DOWN = 1, XDOS_PAINT_MOVE, XDOS_PAINT_UP, XDOS_PAINT_CLEAR };

typedef struct XdosSurface {
    uint16_t *pixels;
    int width, height, stride;
} XdosSurface;
typedef int32_t (*XdosStroke)(void *state, const XdosSurface *surface, int event, int x, int y);
typedef int32_t (*XdosWorkspaceRequest)(void *context, int operation, const char *text);
typedef int32_t (*XdosWorkspaceAction)(XdosWorkspaceRequest request, void *context,
                                       int action, const char *text);
typedef struct XdosWorkspaceApi {
    XdosEditorApi editor;
    int32_t (*open_canvas)(void *context, const char *title, XdosStroke stroke);
    int32_t (*open_workspace)(void *context, int panel, const char *title,
                              const char *hint, const char *buttons,
                              XdosWorkspaceAction action);
} XdosWorkspaceApi;

#endif
