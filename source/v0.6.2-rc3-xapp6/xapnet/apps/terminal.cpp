#include <lvgl.h>
#include "xdos_corejobs.h"
#include "xdos_terminal_v2.h"
#include "xdos_text_keys.h"

namespace {
const XdosTerminalV2Api *api;
lv_obj_t *entry,*output;
lv_timer_t *poll;
int slot;
unsigned revision=~0u;
void Refresh(lv_timer_t *)
{
    const unsigned current=api->revision(api->base.context);
    if (revision==current) return;
    revision=current;
    lv_textarea_set_text(output,api->text(api->base.context));
    lv_textarea_set_cursor_pos(output,LV_TEXTAREA_CURSOR_LAST);
}
void Run()
{
    const char *text=lv_textarea_get_text(entry);
    if (*text && !api->run(api->base.context,text)) lv_textarea_set_text(entry,"");
    Refresh(nullptr);
}
void Key(void *,unsigned key)
{
    switch (key) {
    case 13: Run(); break;
    case 8: lv_textarea_delete_char(entry); break;
    case XdosKeyLeft: lv_textarea_cursor_left(entry); break;
    case XdosKeyRight: lv_textarea_cursor_right(entry); break;
    case XdosKeyHome: lv_textarea_set_cursor_pos(entry,0); break;
    case XdosKeyEnd: lv_textarea_set_cursor_pos(entry,LV_TEXTAREA_CURSOR_LAST); break;
    case XdosKeyDelete: lv_textarea_delete_char_forward(entry); break;
    default: if (key>=32 && key<=126) lv_textarea_add_char(entry,key); break;
    }
}
void Close(void *)
{
    lv_timer_delete(poll);
    xdos_v2_exit(slot);
}
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version!=XDOS_ABI_VERSION || base->struct_bytes!=sizeof(XdosTerminalV2Api))
        return -1;
    api=reinterpret_cast<const XdosTerminalV2Api *>(base);
    if (api->capability!=XDOS_TERMINAL_V2_CAPABILITY) return -1;
    slot=xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    auto *body=slot>=0 ? xdos_v2_window(slot,"Terminal",640,416) : nullptr;
    if (!body) return -1;
    entry=lv_textarea_create(body);
    lv_textarea_set_one_line(entry,true);
    lv_textarea_set_max_length(entry,159);
    lv_textarea_set_placeholder_text(entry,"Command (HELP for commands)");
    lv_obj_set_width(entry,lv_pct(78));
    auto *button=lv_button_create(body);
    lv_obj_align(button,LV_ALIGN_TOP_RIGHT,0,0);
    auto *name=lv_label_create(button);
    lv_label_set_text(name,"RUN");
    lv_obj_center(name);
    lv_obj_add_event_cb(button,[](lv_event_t *) { Run(); },LV_EVENT_CLICKED,nullptr);
    output=lv_textarea_create(body);
    lv_obj_set_size(output,lv_pct(100),lv_pct(82));
    lv_obj_align(output,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_remove_flag(output,LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_textarea_set_cursor_click_pos(output,false);
    poll=lv_timer_create(Refresh,70,nullptr);
    if (!poll) return -1;
    xdos_v2_register_key(slot,Key,nullptr);
    xdos_v2_register_close(slot,Close,nullptr);
    Refresh(nullptr);
    return 0;
}
