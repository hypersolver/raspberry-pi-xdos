#include "xapp_workspace.h"

static int32_t action(XdosWorkspaceRequest request, void *context, int index, const char *text)
{
#if PANEL == 1
    if (index == 0) return request(context, XDOS_REQUEST_LIST, 0);
    if (index == 1) return request(context, XDOS_REQUEST_LIST_USB, 0);
    if (index == 2 && text && *text) return request(context, XDOS_REQUEST_READ, text);
#elif PANEL == 2
    if (index == 0 && text && *text) return request(context, XDOS_REQUEST_HTTP, text);
    if (index == 1) return request(context, XDOS_REQUEST_CANCEL_HTTP, 0);
#elif PANEL == 3
    if (index == 0 && text && *text) return request(context, XDOS_REQUEST_SELECT_IMAGE, text);
    if (index == 1) return request(context, XDOS_REQUEST_RESTORE_BOOT, 0);
    if (index == 2 && text && text[0] == 'R' && text[1] == 'E' && text[2] == 'S' &&
        text[3] == 'E' && text[4] == 'T' && !text[5])
        return request(context, XDOS_REQUEST_RESET, 0);
#endif
    return -1;
}

int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosWorkspaceApi)) return -1;
    const XdosWorkspaceApi *api = (const XdosWorkspaceApi *)base;
#if PANEL == 1
    return api->open_workspace(base->context, XDOS_PANEL_FILES, "Files",
        "Type a filename under SD:/XDOS to read", "LIST SD|LIST USB|READ", action);
#elif PANEL == 2
    return api->open_workspace(base->context, XDOS_PANEL_HTTP, "Browser",
        "HTTP URL", "OPEN|STOP", action);
#elif PANEL == 3
    return api->open_workspace(base->context, XDOS_PANEL_UPDATE, "XDOS Update",
        "Name and CRC32, e.g. NEXT.IMG 1234abcd. To restart type RESET.",
        "SELECT IMAGE|RESTORE BOOT|RESTART", action);
#else
#error Invalid PANEL
#endif
}
