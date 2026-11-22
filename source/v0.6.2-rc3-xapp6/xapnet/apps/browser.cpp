#include <lvgl.h>
#include "xdos_corejobs.h"
#include "xdos_browser_v2.h"
#include "xdos_text_keys.h"

extern "C" void xdos_browser_fetch(void *,void *,const char *,unsigned);

namespace {
const XdosBrowserV2Api *api;
lv_obj_t *entry,*output,*message;
bool closing;
lv_timer_t *poll;
int slot;
unsigned revision=~0u;
void Refresh(lv_timer_t *)
{
    if (closing) {
        if (!api->busy()) {
            lv_timer_delete(poll);
            api->claim(false);
            xdos_v2_exit(slot);
        }
        return;
    }
    const unsigned current=api->revision(api->base.context);
    if (revision==current) return;
    revision=current;
    lv_textarea_set_text(output,api->text(api->base.context));
    lv_textarea_set_cursor_pos(output,0);
    lv_label_set_text(message,api->status(api->base.context));
}
void Run()
{
    const char *text=lv_textarea_get_text(entry);
    if (!closing && *text) api->run(api->base.context,text,xdos_browser_fetch);
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
    closing=true;
    api->cancel(api->base.context);
    lv_obj_add_state(lv_obj_get_parent(entry),LV_STATE_DISABLED);
    Refresh(nullptr);
}
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version!=XDOS_ABI_VERSION || base->struct_bytes!=sizeof(XdosBrowserV2Api))
        return -1;
    api=reinterpret_cast<const XdosBrowserV2Api *>(base);
    if (api->capability!=XDOS_BROWSER_V2_CAPABILITY || !api->claim(true)) return -1;
    slot=xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    auto *body=slot>=0 ? xdos_v2_window(slot,"Browser",720,460) : nullptr;
    if (!body) { api->claim(false); return -1; }
    entry=lv_textarea_create(body);
    lv_textarea_set_one_line(entry,true);
    lv_textarea_set_max_length(entry,191);
    lv_textarea_set_placeholder_text(entry,"HTTP URL");
    lv_obj_set_width(entry,lv_pct(65));
    auto *button=lv_button_create(body);
    lv_obj_align(button,LV_ALIGN_TOP_RIGHT,0,0);
    auto *name=lv_label_create(button);
    lv_label_set_text(name,"OPEN");
    lv_obj_center(name);
    lv_obj_add_event_cb(button,[](lv_event_t *) { Run(); },LV_EVENT_CLICKED,nullptr);
    lv_obj_set_x(button,-85);
    auto *stop=lv_button_create(body);
    lv_obj_align(stop,LV_ALIGN_TOP_RIGHT,0,0);
    auto *stopLabel=lv_label_create(stop);
    lv_label_set_text(stopLabel,"STOP");
    lv_obj_center(stopLabel);
    lv_obj_add_event_cb(stop,[](lv_event_t *) { api->cancel(api->base.context); },LV_EVENT_CLICKED,nullptr);
    message=lv_label_create(body);
    lv_obj_set_width(message,lv_pct(100));
    lv_label_set_long_mode(message,LV_LABEL_LONG_DOT);
    lv_obj_set_pos(message,0,48);
    output=lv_textarea_create(body);
    lv_obj_set_size(output,lv_pct(100),lv_pct(81));
    lv_obj_align(output,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_remove_flag(output,LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_textarea_set_cursor_click_pos(output,false);
    poll=lv_timer_create(Refresh,70,nullptr);
    if (!poll) { api->claim(false); return -1; }
    xdos_v2_register_key(slot,Key,nullptr);
    xdos_v2_register_close(slot,Close,nullptr);
    Refresh(nullptr);
    return 0;
}
