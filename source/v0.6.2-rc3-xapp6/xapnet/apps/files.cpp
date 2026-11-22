#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "xdos_corejobs.h"
#include "xdos_xapp_memory.h"
#include "xdos_files_v2.h"

namespace {
void StyleWindowSurface(lv_obj_t *obj, unsigned background, unsigned foreground,
                        unsigned border, int borderWidth)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(foreground), 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(obj, borderWidth, 0);
    lv_obj_set_style_radius(obj, 0, 0);
}

lv_obj_t *MakeWindowButton(lv_obj_t *parent, const char *label,
                           lv_event_cb_t event, void *owner)
{
    auto *button = lv_button_create(parent);
    auto *text = lv_label_create(button);
    lv_label_set_text(text,label);
    lv_obj_center(text);
    lv_obj_add_event_cb(button,event,LV_EVENT_CLICKED,owner);
    StyleWindowSurface(button, 0x294A6A, 0xFFFFFF, 0xB6C4D3, 1);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x4A7491), LV_STATE_PRESSED);
    return button;
}

lv_obj_t *MakeWindowTextArea(lv_obj_t *parent)
{
    auto *area = lv_textarea_create(parent);
    StyleWindowSurface(area, 0xFFFFFF, 0x203047, 0xB6C4D3, 1);
    return area;
}

constexpr unsigned FilePageSize=XDOS_DIRECTORY_LIMIT;
constexpr unsigned FileHistorySize=12;
struct FileBrowser {
    lv_obj_t *tree;
    lv_obj_t *list;
    lv_obj_t *address;
    lv_obj_t *notice;
    lv_obj_t *preview;
    lv_obj_t *back;
    lv_obj_t *forward;
    lv_obj_t *previousPage;
    lv_obj_t *nextPage;
    unsigned page;
    unsigned position;
    unsigned count;
    char history[FileHistorySize][512];
    char deleteTarget[512];
    char launchTarget[512];
    uint32_t launchClickTick;
    lv_obj_t *selectedRow;
};
FileBrowser browserState;
const XdosFilesV2Api *api;
int slot;
void RenderFiles(void *context);
void NavigateFiles(void *context, const char *path);
void FileTreeClick(lv_event_t *event);

struct FileTreeNode {
    lv_obj_t *preview;
    lv_obj_t *children;
    char path[512];
    int slot;
    bool directory;
    bool tree;
};

void StyleTreeColumn(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_flex_flow(obj,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(obj,2,0);
    lv_obj_set_scroll_dir(obj,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(obj,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
}

unsigned CountTreeRows(lv_obj_t *column)
{
    unsigned count=0;
    for (unsigned i=0;i<lv_obj_get_child_count(column);++i) {
        auto *group=lv_obj_get_child(column,i);
        if (lv_obj_get_child_count(group)==0) continue; // status label
        ++count;
        if (lv_obj_get_child_count(group)>1)
            count+=CountTreeRows(lv_obj_get_child(group,1));
    }
    return count;
}

void PreviewTreeFile(FileTreeNode &node)
{
    char buffer[4097];
    unsigned read=0;
    int more=0;
    if (api->preview(node.path, buffer, sizeof buffer - 1, &read, &more)) {
        lv_textarea_set_text(node.preview,"Unable to read file.");
        return;
    }
    for (unsigned i=0;i<read;++i) {
        const unsigned char c=buffer[i];
        if (!c || (c<32 && c!='\r' && c!='\n' && c!='\t')) {
            lv_textarea_set_text(node.preview,"Binary file. Text preview unavailable.");
            return;
        }
    }
    buffer[read]=0;
    lv_textarea_set_text(node.preview,buffer);
    if (more) lv_textarea_add_text(node.preview,"\n[Preview limited to 4 KiB]");
    lv_textarea_set_cursor_pos(node.preview,0);
}

void AddFileTreeNode(lv_obj_t *parent,lv_obj_t *preview,
                              const char *label,const char *path,bool directory)
{
    auto *browser=static_cast<FileBrowser *>(lv_obj_get_user_data(preview));
    auto *node=static_cast<FileTreeNode *>(xdos_v2_alloc(slot,sizeof(FileTreeNode)));
    if (!node) return;
    *node={preview,nullptr,{},slot,directory,parent!=browser->list};
    strcpy(node->path,path);
    auto *group=lv_obj_create(parent);
    StyleTreeColumn(group);
    lv_obj_remove_flag(group,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(group,lv_pct(100),LV_SIZE_CONTENT);
    auto *button=lv_button_create(group);
    StyleWindowSurface(button,0xFFFFFF,0x203047,0xB6C4D3,1);
    lv_obj_set_style_bg_color(button,lv_color_hex(0x1F5FAF),LV_STATE_CHECKED);
    lv_obj_set_style_text_color(button,lv_color_hex(0xFFFFFF),LV_STATE_CHECKED);
    lv_obj_set_style_pad_all(button,4,0);
    lv_obj_set_size(button,lv_pct(100),27);
    auto *name=lv_label_create(button);
    lv_obj_set_width(name,lv_pct(100));
    lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    lv_label_set_text_fmt(name,"%s %s",directory ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE,label);
    lv_obj_align(name,LV_ALIGN_LEFT_MID,0,0);
    lv_obj_add_event_cb(button,FileTreeClick,LV_EVENT_SHORT_CLICKED,node);
    if (!directory) lv_obj_add_event_cb(button,[](lv_event_t *event) {
        auto &item = *static_cast<FileTreeNode *>(lv_event_get_user_data(event));
        auto &browser = *static_cast<FileBrowser *>(lv_obj_get_user_data(item.preview));
        browser.launchTarget[0] = 0;
        strcpy(browser.deleteTarget, item.path);
        lv_textarea_set_text(item.preview, "Confirm deletion with Delete:\n");
        lv_textarea_add_text(item.preview, item.path);
    },LV_EVENT_LONG_PRESSED,node);
    lv_obj_add_event_cb(button,[](lv_event_t *event) {
        auto *node=static_cast<FileTreeNode *>(lv_event_get_user_data(event));
        xdos_v2_free(node->slot,node);
    },LV_EVENT_DELETE,node);
}

void FileTreeClick(lv_event_t *event)
{
    auto &node=*static_cast<FileTreeNode *>(lv_event_get_user_data(event));
    auto *browser=static_cast<FileBrowser *>(lv_obj_get_user_data(node.preview));
    browser->deleteTarget[0] = 0;
    if (!node.directory) {
        if (browser->selectedRow)
            lv_obj_remove_state(browser->selectedRow, LV_STATE_CHECKED);
        browser->selectedRow = lv_event_get_target_obj(event);
        lv_obj_add_state(browser->selectedRow, LV_STATE_CHECKED);
        const size_t size=strlen(node.path);
        if (size>5 && !strcasecmp(node.path+size-5,".XAPP")) {
            constexpr uint32_t doubleClickMs = 400;
            const bool doubleClick = !strcmp(browser->launchTarget, node.path)
                && lv_tick_elaps(browser->launchClickTick) <= doubleClickMs;
            if (!doubleClick) {
                strcpy(browser->launchTarget, node.path);
                browser->launchClickTick = lv_tick_get();
                lv_textarea_set_text(node.preview,"Double-click to run:\n");
                lv_textarea_add_text(node.preview,node.path);
                return;
            }
            browser->launchTarget[0] = 0;
            if (api->launch(node.path))
                lv_textarea_set_text(node.preview,"Unable to queue XAPP.");
        } else {
            browser->launchTarget[0] = 0;
            PreviewTreeFile(node);
        }
        return;
    }
    // Copy before changing the list because navigation deletes its old rows.
    char destination[sizeof node.path];
    strcpy(destination,node.path);
    const bool fromTree=node.tree;
    NavigateFiles(browser,destination);
    if (!fromTree) return;
    if (node.children) {
        lv_obj_delete(node.children);
        node.children=nullptr;
        return;
    }
    const unsigned used=CountTreeRows(browser->tree);
    if (used>=XDOS_DIRECTORY_LIMIT) {
        lv_label_set_text(browser->notice,"Directory tree limit reached. Collapse a branch to expand more.");
        return;
    }
    node.children=lv_obj_create(lv_obj_get_parent(lv_event_get_target_obj(event)));
    StyleTreeColumn(node.children);
    lv_obj_remove_flag(node.children,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(node.children,12,0);
    lv_obj_set_size(node.children,lv_pct(100),LV_SIZE_CONTENT);

    unsigned listed=0;
    int more=0;
    const int result=api->visit(node.path,0,XDOS_DIRECTORY_LIMIT-used,1,
        [](void *context,const char *name,int) {
            auto &parent=*static_cast<FileTreeNode *>(context);
            char path[sizeof parent.path];
            const int length=snprintf(path,sizeof path,"%s%s/",parent.path,name);
            if (length>=0 && unsigned(length)<sizeof path)
                AddFileTreeNode(parent.children,parent.preview,name,path,true);
        },&node,&listed,&more);
    if (!listed || more || result!=0) {
        auto *label=lv_label_create(node.children);
        lv_label_set_text(label,result!=0 ? "Directory read failed." :
                          more ? "More folders in file list." : "(no subfolders)");
    }
}

void RenderFiles(void *context)
{
    auto &browser=*static_cast<FileBrowser *>(context);
    browser.deleteTarget[0] = 0;
    browser.launchTarget[0] = 0;
    if (browser.selectedRow) lv_obj_remove_state(browser.selectedRow, LV_STATE_CHECKED);
    browser.selectedRow = nullptr;
    const char *path=browser.history[browser.position];
    lv_label_set_text(browser.address,path);
    lv_obj_clean(browser.list);
    lv_label_set_text(browser.notice,"");
    lv_obj_scroll_to_y(browser.list,0,LV_ANIM_OFF);
    lv_textarea_set_text(browser.preview,"Select a file to preview. Double-click an .XAPP to launch.");
    auto enabled=[](lv_obj_t *obj,bool value) {
        if (value) lv_obj_remove_state(obj,LV_STATE_DISABLED);
        else lv_obj_add_state(obj,LV_STATE_DISABLED);
    };
    enabled(browser.back,browser.position>0);
    enabled(browser.forward,browser.position+1<browser.count);
    enabled(browser.previousPage,browser.page>0);
    enabled(browser.nextPage,false);
    unsigned entryIndex=0;
    int more=0;
    const int result=api->visit(path,browser.page*FilePageSize,FilePageSize,0,
        [](void *context,const char *name,int directory) {
            auto &b=*static_cast<FileBrowser *>(context);
            char child[sizeof b.history[0]];
            const int length=snprintf(child,sizeof child,"%s%s%s",b.history[b.position],
                                      name,directory ? "/" : "");
            if (length>=0 && unsigned(length)<sizeof child)
                AddFileTreeNode(b.list,b.preview,name,child,directory);
        },&browser,&entryIndex,&more);
    enabled(browser.nextPage,more);
    if (more || browser.page)
        lv_label_set_text(browser.notice,"Folder exceeds 128 entries. Showing up to 128 per page.");
    if (result!=0 || !entryIndex) {
        lv_textarea_set_text(browser.preview,result!=0 ? "Directory read failed." : "Folder is empty.");
    }
}

void NavigateFiles(void *context,const char *path)
{
    auto &b=*static_cast<FileBrowser *>(context);
    if (!b.count || strcmp(path,b.history[b.position])) {
        if (b.count && b.position+1==FileHistorySize) {
            memmove(b.history,b.history+1,sizeof b.history-sizeof b.history[0]);
            --b.position;
        }
        if (b.count) ++b.position;
        strcpy(b.history[b.position],path);
        b.count=b.position+1;
    }
    b.page=0;
    RenderFiles(&b);
}

void OpenFileTree(lv_obj_t *body)
{
    auto *browser=&browserState;
    lv_obj_set_style_bg_color(body,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_bg_opa(body,LV_OPA_COVER,0);
    auto *bar=lv_obj_create(body);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar,lv_pct(98),62);
    lv_obj_align(bar,LV_ALIGN_TOP_MID,0,3);

    auto nav=[](lv_event_t *event) {
        auto *b=static_cast<FileBrowser *>(lv_event_get_user_data(event));
        auto *target=lv_event_get_target_obj(event);
        if (target==b->back && b->position) --b->position;
        else if (target==b->forward && b->position+1<b->count) ++b->position;
        else if (target==b->previousPage && b->page) { --b->page; RenderFiles(b); return; }
        else if (target==b->nextPage) { ++b->page; RenderFiles(b); return; }
        else if (target!=b->back && target!=b->forward &&
                 target!=b->previousPage) {
            auto *label=lv_obj_get_child(target,0);
            if (!strcmp(lv_label_get_text(label),"Up")) {
                char parent[sizeof b->history[0]];
                strcpy(parent,b->history[b->position]);
                size_t length=strlen(parent);
                if (length<=4) return;
                --length;
                while (length && parent[length-1]!='/') --length;
                if (!length) return;
                parent[length]=0;
                NavigateFiles(b,parent);
                return;
            }
        }
        b->page=0;
        RenderFiles(b);
    };
    const char *labels[]={"Back","Forward","Up","Refresh","Prev","Next"};
    lv_obj_t *buttons[6];
    lv_obj_update_layout(body);
    const int buttonWidth=LV_MIN(106,lv_obj_get_width(body)/7);
    for (int i=0;i<6;++i) {
        buttons[i]=MakeWindowButton(bar,labels[i],nav,browser);
        lv_obj_set_size(buttons[i],buttonWidth-4,27);
        lv_obj_align(buttons[i],LV_ALIGN_TOP_LEFT,i*buttonWidth,0);
    }
    browser->back=buttons[0];
    browser->forward=buttons[1];
    browser->previousPage=buttons[4];
    browser->nextPage=buttons[5];
    auto *remove = MakeWindowButton(bar,"Delete",[](lv_event_t *event) {
        auto *b = static_cast<FileBrowser *>(lv_event_get_user_data(event));
        if (!b->deleteTarget[0]) {
            lv_textarea_set_text(b->preview,"Hold a file row to select it for deletion.");
            return;
        }
        const auto result = api->remove(b->deleteTarget);
        b->deleteTarget[0] = 0;
        if (result == 0) RenderFiles(b);
        lv_textarea_set_text(b->preview,result == 0 ? "File deleted."
                                                      : "Delete failed or path is protected.");
    },browser);
    lv_obj_set_size(remove,buttonWidth-4,27);
    lv_obj_align(remove,LV_ALIGN_TOP_LEFT,6*buttonWidth,0);
    browser->address=lv_label_create(bar);
    lv_obj_set_width(browser->address,lv_pct(100));
    lv_label_set_long_mode(browser->address,LV_LABEL_LONG_DOT);
    lv_obj_align(browser->address,LV_ALIGN_TOP_LEFT,4,34);

    lv_obj_update_layout(body);
    const int paneHeight=LV_MAX(100,lv_obj_get_height(body)-92);
    auto *tree=browser->tree=lv_obj_create(body);
    StyleTreeColumn(tree);
    lv_obj_set_style_bg_color(tree,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_bg_opa(tree,LV_OPA_COVER,0);
    lv_obj_set_style_pad_all(tree,5,0);
    lv_obj_set_size(tree,lv_pct(27),paneHeight);
    lv_obj_align(tree,LV_ALIGN_TOP_LEFT,8,65);
    auto *list=browser->list=lv_obj_create(body);
    StyleTreeColumn(list);
    lv_obj_set_style_width(list,5,LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list,lv_color_hex(0x698AA2),LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list,LV_OPA_COVER,LV_PART_SCROLLBAR);
    lv_obj_set_style_rotary_sensitivity(list,256,LV_PART_MAIN);
    lv_obj_set_size(list,lv_pct(69),paneHeight*3/5);
    lv_obj_align(list,LV_ALIGN_TOP_RIGHT,-8,65);
    auto *preview=browser->preview=MakeWindowTextArea(body);
    lv_obj_set_size(preview,lv_pct(69),paneHeight*2/5-4);
    lv_obj_align(preview,LV_ALIGN_TOP_RIGHT,-8,65+paneHeight*3/5+4);
    lv_obj_set_user_data(preview,browser);
    lv_obj_remove_flag(preview,LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_textarea_set_cursor_click_pos(preview,false);
    browser->notice=lv_label_create(body);
    lv_obj_set_width(browser->notice,lv_pct(96));
    lv_label_set_long_mode(browser->notice,LV_LABEL_LONG_DOT);
    lv_obj_align(browser->notice,LV_ALIGN_BOTTOM_LEFT,8,-3);
    AddFileTreeNode(tree,preview,"SD Card","SD:/",true);
    AddFileTreeNode(tree,preview,"USB Drive","USB:/",true);
    api->setPanes(tree,list);
    NavigateFiles(browser,"SD:/");
}

void Close(void *)
{
    api->setPanes(nullptr,nullptr);
    xdos_v2_exit(slot);
}
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *base)
{
    if (!base || base->abi_version!=XDOS_ABI_VERSION || base->struct_bytes!=sizeof(XdosFilesV2Api))
        return -1;
    api=reinterpret_cast<const XdosFilesV2Api *>(base);
    if (api->capability!=XDOS_FILES_V2_CAPABILITY || !api->claim()) return -1;
    slot=xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    auto *body=slot>=0 ? xdos_v2_window(slot,"Arduino Files",750,550) : nullptr;
    if (!body) {
        api->setPanes(nullptr,nullptr);
        return -1;
    }
    xdos_v2_register_close(slot,Close,nullptr);
    OpenFileTree(body);
    return 0;
}
