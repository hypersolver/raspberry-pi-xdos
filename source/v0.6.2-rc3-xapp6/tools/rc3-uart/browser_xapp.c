#include <xapp_workspace.h>

extern void xdos_browser_fetch(void *network, void *desktop, const char *url, unsigned id);

static int32_t action(XdosWorkspaceRequest request, void *context, int index, const char *text)
{
    if (index == 0 && text && *text) return request(context, XDOS_REQUEST_HTTP, text);
    if (index == 1) return request(context, XDOS_REQUEST_CANCEL_HTTP, 0);
    return -1;
}

int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosWorkspaceApi)) return -1;
    const XdosWorkspaceApi *api = (const XdosWorkspaceApi *)base;
    int32_t result=api->register_browser_fetch(base->context,xdos_browser_fetch);
    if (result) return result;
    return api->open_workspace(base->context,XDOS_PANEL_HTTP,"Browser",
                               "HTTP URL","OPEN|STOP",action);
}
