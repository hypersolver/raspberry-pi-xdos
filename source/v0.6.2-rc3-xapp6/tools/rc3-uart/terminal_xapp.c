#include <xapp_workspace.h>

static int32_t action(XdosWorkspaceRequest request, void *context, int index, const char *text)
{
    if (index != 0 || !text || !*text) return -1;
    return request(context, XDOS_REQUEST_TERMINAL, text);
}

int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosWorkspaceApi)) return -1;
    const XdosWorkspaceApi *api = (const XdosWorkspaceApi *)base;
    return api->open_workspace(base->context, XDOS_PANEL_TERMINAL, "Terminal",
                               "Command (HELP for commands)", "RUN", action);
}
