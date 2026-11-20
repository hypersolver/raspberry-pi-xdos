#include "xapp_editor.h"

static int32_t action(XdosNoteRequest request, void *context, int operation,
                      const char *text)
{
    if (operation == XDOS_EDITOR_LOAD)
        return request(context, XDOS_EDITOR_LOAD, 0);
    if (operation != XDOS_EDITOR_SAVE || !text) return -1;
    unsigned length = 0;
    while (text[length]) {
        if (++length > 2047) return -1;
    }
    return request(context, XDOS_EDITOR_SAVE, text);
}

int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION ||
        base->struct_bytes < sizeof(XdosEditorApi)) return -1;
    const XdosEditorApi *api = (const XdosEditorApi *)base;
    return api->open_editor(base->context, "Notepad - NOTE.TXT", action);
}
