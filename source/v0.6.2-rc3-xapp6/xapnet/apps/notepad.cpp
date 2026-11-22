#include <lvgl.h>
#include "xdos_corejobs.h"
#include "xdos_note_v2.h"
#include "xdos_text_keys.h"

extern "C" int32_t xapp_entry(const XdosHostApiV1 *);

namespace {
enum class Pending { None, Load, Save };
struct Editor {
    const XdosNoteV2Api *api;
    lv_obj_t *text, *status;
    lv_timer_t *timer;
    unsigned revision, noteRevision;
    int slot;
    bool overwrite;
    Pending pending;
};
Editor editor;

unsigned LineEdge(bool end)
{
    const unsigned cursor = lv_textarea_get_cursor_pos(editor.text);
    unsigned position = 0, start = 0;
    for (auto *p = lv_textarea_get_text(editor.text); *p; ++p) {
        if ((static_cast<unsigned char>(*p) & 0xc0) == 0x80) continue;
        if (*p == '\n') {
            if (position >= cursor) return end ? position : start;
            start = position + 1;
        }
        ++position;
    }
    return end ? position : start;
}

void Request(Pending action)
{
    if (editor.pending != Pending::None) return;
    const auto &api = *editor.api;
    XdosNoteSnapshot state{};
    api.snapshot(api.base.context, &state);
    const bool load = action == Pending::Load;
    const int result = load
        ? api.load(api.base.context)
        : api.save(api.base.context, lv_textarea_get_text(editor.text));
    if (result) {
        lv_label_set_text(editor.status, "SD busy, unavailable, or note too long");
        return;
    }
    editor.revision = state.storage_revision;
    editor.noteRevision = state.note_revision;
    editor.pending = action;
    lv_label_set_text(editor.status, load ? "Loading..." : "Saving...");
}

void Poll(lv_timer_t *)
{
    if (editor.pending == Pending::None) return;
    XdosNoteSnapshot state{};
    editor.api->snapshot(editor.api->base.context, &state);
    if (state.storage_revision == editor.revision) return;
    if (editor.pending == Pending::Load && state.note_revision != editor.noteRevision)
        lv_textarea_set_text(editor.text, state.text);
    lv_label_set_text(editor.status, state.status);
    if (editor.api->base.log)
        editor.api->base.log(editor.api->base.context, "[NOTE V2] Storage request completed\r\n");
    editor.pending = Pending::None;
}

void Key(void *, unsigned key)
{
    if (key == 19) { Request(Pending::Save); return; }
    if (key == 15) { Request(Pending::Load); return; }
    if (editor.pending == Pending::Load) return;

    switch (key) {
    case XdosKeyLeft: lv_textarea_cursor_left(editor.text); return;
    case XdosKeyRight: lv_textarea_cursor_right(editor.text); return;
    case XdosKeyUp: lv_textarea_cursor_up(editor.text); return;
    case XdosKeyDown: lv_textarea_cursor_down(editor.text); return;
    case XdosKeyHome:
    case XdosKeyEnd: lv_textarea_set_cursor_pos(editor.text, LineEdge(key == XdosKeyEnd)); return;
    case XdosKeyDelete: lv_textarea_delete_char_forward(editor.text); return;
    case XdosKeyInsert: editor.overwrite = !editor.overwrite; return;
    case 8:
    case 127: lv_textarea_delete_char(editor.text); return;
    case 10:
    case 13: lv_textarea_add_char(editor.text, '\n'); return;
    default: break;
    }
    if (key < 32 || key > 126) return;
    if (editor.overwrite && lv_textarea_get_cursor_pos(editor.text) < LineEdge(true))
        lv_textarea_delete_char_forward(editor.text);
    lv_textarea_add_char(editor.text, key);
}

void SaveClick(lv_event_t *) { Request(Pending::Save); }
void LoadClick(lv_event_t *) { Request(Pending::Load); }

void AddButton(lv_obj_t *parent, const char *name, lv_event_cb_t handler, int x)
{
    auto *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, 0);
    auto *label = lv_label_create(button);
    lv_label_set_text(label, name);
    lv_obj_add_event_cb(button, handler, LV_EVENT_CLICKED, nullptr);
}

void Close(void *)
{
    lv_timer_delete(editor.timer);
    xdos_v2_exit(editor.slot);
}
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version != XDOS_ABI_VERSION || base->struct_bytes < sizeof(XdosNoteV2Api))
        return -1;
    const auto *api = reinterpret_cast<const XdosNoteV2Api *>(base);
    if (api->capability != XDOS_NOTE_V2_CAPABILITY) return -1;
    editor.slot = xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    if (editor.slot < 0) return -1;
    editor.api = api;

    auto *body = xdos_v2_window(editor.slot, "Notepad - NOTE.TXT", 640, 420);
    if (!body) return -1;
    AddButton(body, "Save", SaveClick, 0);
    AddButton(body, "Load", LoadClick, 76);
    editor.text = lv_textarea_create(body);
    lv_textarea_set_max_length(editor.text, XDOS_NOTE_MAX_BYTES);
    lv_obj_set_size(editor.text, lv_pct(100), lv_pct(68));
    lv_obj_set_pos(editor.text, 0, 44);
    lv_obj_add_state(editor.text, LV_STATE_FOCUSED);
    editor.status = lv_label_create(body);
    lv_obj_align(editor.status, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_label_set_text(editor.status, "Ctrl+S Save   Ctrl+O Load   Alt+F4 Close");
    editor.timer = lv_timer_create(Poll, 50, nullptr);
    if (!editor.timer) return -1;
    xdos_v2_register_close(editor.slot, Close, nullptr);
    xdos_v2_register_key(editor.slot, Key, nullptr);
    Request(Pending::Load);
    return 0;
}
