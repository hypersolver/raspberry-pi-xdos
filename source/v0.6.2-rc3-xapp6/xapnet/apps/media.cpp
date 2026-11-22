#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "xdos_corejobs.h"
#include "xdos_media_v2.h"
#include "xdos_text_keys.h"

namespace {
#ifdef XDOS_MUSIC_APP
constexpr bool Music=true;
#else
constexpr bool Music=false;
#endif
const XdosMediaV2Api *api;
lv_obj_t *field,*status;
int slot;
bool playing;

void Play(bool jack)
{
    const char *path=lv_textarea_get_text(field);
    const int error=jack ? api->jack(path) : api->play(path);
    if (!error) playing=true;
    lv_label_set_text(status,error ? "Unable to play this file." : "Playback requested.");
}
void Stop()
{
    if (playing && api->stop) api->stop();
    playing=false;
}
void Close(void *) { Stop(); xdos_v2_exit(slot); }
void Key(void *,unsigned key)
{
    switch (key) {
    case 13: Play(false); break;
    case 8: lv_textarea_delete_char(field); break;
    case XdosKeyLeft: lv_textarea_cursor_left(field); break;
    case XdosKeyRight: lv_textarea_cursor_right(field); break;
    case XdosKeyHome: lv_textarea_set_cursor_pos(field,0); break;
    case XdosKeyEnd: lv_textarea_set_cursor_pos(field,LV_TEXTAREA_CURSOR_LAST); break;
    case XdosKeyDelete: lv_textarea_delete_char_forward(field); break;
    default: if (key>=32 && key<=126) lv_textarea_add_char(field,key); break;
    }
}
void Button(lv_obj_t *body,const char *title,int x,lv_event_cb_t callback)
{
    auto *button=lv_button_create(body);
    lv_obj_set_pos(button,x,49);
    lv_obj_set_width(button,95);
    auto *label=lv_label_create(button);
    lv_label_set_text(label,title);
    lv_obj_center(label);
    lv_obj_add_event_cb(button,callback,LV_EVENT_CLICKED,nullptr);
}
struct Picker { lv_obj_t *obj; const char *folder; unsigned listed; };
void AddMedia(void *context,const char *name,int directory)
{
    auto &picker=*static_cast<Picker *>(context);
    const size_t size=strlen(name);
    if (directory || size<5 || picker.listed>=XDOS_DIRECTORY_LIMIT || strchr(name,'\n')) return;
    const char *extension=name+size-4;
    if (Music ? (strcasecmp(extension,".mp3") && strcasecmp(extension,".wav"))
              : strcasecmp(extension,".mp4")) return;
    char path[192];
    const int length=snprintf(path,sizeof path,"%s%s",picker.folder,name);
    if (length<0 || static_cast<unsigned>(length)>=sizeof path) return;
    lv_dropdown_add_option(picker.obj,path,LV_DROPDOWN_POS_LAST);
    ++picker.listed;
}
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version!=XDOS_ABI_VERSION || base->struct_bytes!=sizeof(XdosMediaV2Api)) return -1;
    api=reinterpret_cast<const XdosMediaV2Api *>(base);
    if (api->capability!=XDOS_MEDIA_V2_CAPABILITY || !api->play || (Music && !api->jack)) return -1;
    slot=xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    auto *body=slot>=0 ? xdos_v2_window(slot,Music ? "MP3 Player" : "MP4 Player",610,250) : nullptr;
    if (!body) return -1;
    field=lv_textarea_create(body);
    lv_obj_set_width(field,lv_pct(100));
    lv_textarea_set_one_line(field,true);
    lv_textarea_set_max_length(field,191);
    lv_textarea_set_placeholder_text(field,Music ? "MP3 or 44.1 kHz PCM WAV" : "MP4 path on SD");
    Button(body,Music ? "HDMI" : "PLAY",0,[](lv_event_t *) { Play(false); });
    if (Music) {
        Button(body,"3.5 MM",104,[](lv_event_t *) { Play(true); });
        Button(body,"STOP",208,[](lv_event_t *) { Stop(); lv_label_set_text(status,"Stopped."); });
    }
    auto *dropdown=lv_dropdown_create(body);
    lv_obj_set_width(dropdown,lv_pct(100));
    lv_obj_set_pos(dropdown,0,102);
    lv_dropdown_set_options(dropdown,Music ? "Choose MP3 or WAV from SD" : "Choose an MP4 from SD");
    Picker picker{dropdown,nullptr,0};
    const char *folders[]={"SD:/","SD:/XDOS/INBOX/","SD:/XDOS/VIDEO/","SD:/XDOS/MUSIC/"};
    for (const auto *folder : folders) {
        picker.folder=folder;
        unsigned offset=0,listed=0;
        int more;
        do {
            if (api->visit(folder,offset,XDOS_DIRECTORY_LIMIT,0,AddMedia,&picker,&listed,&more)) break;
            offset+=listed;
        } while (more && listed && picker.listed<XDOS_DIRECTORY_LIMIT);
    }
    lv_obj_add_event_cb(dropdown,[](lv_event_t *event) {
        auto *obj=lv_event_get_target_obj(event);
        if (!lv_dropdown_get_selected(obj)) return;
        char selection[192];
        lv_dropdown_get_selected_str(obj,selection,sizeof selection);
        lv_textarea_set_text(field,selection);
    },LV_EVENT_VALUE_CHANGED,nullptr);
    status=lv_label_create(body);
    lv_obj_align(status,LV_ALIGN_BOTTOM_LEFT,0,0);
    lv_label_set_text(status,"Choose a file or enter its path.");
    xdos_v2_register_key(slot,Key,nullptr);
    xdos_v2_register_close(slot,Close,nullptr);
    return 0;
}
