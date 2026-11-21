#include "xapp_workspace.h"

static int32_t action(XdosWorkspaceRequest request, void *context, int button, const char *path)
{
    if (button==0 && path && *path) return request(context,XDOS_REQUEST_PLAY,path);
    return -1;
}

int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version!=XDOS_ABI_VERSION ||
        base->struct_bytes<sizeof(XdosWorkspaceApi)) return -1;
    const XdosWorkspaceApi *api=(const XdosWorkspaceApi *)base;
    return api->open_workspace(base->context,XDOS_PANEL_PLAYER,"MP4 Player",
                               "SD path or choose a file below","PLAY",action);
}
