
namespace {
constexpr unsigned FilePageSize=XDOS_DIRECTORY_LIMIT;
constexpr unsigned FileHistorySize=12;
struct FileBrowser {
    XDOSLVGL *ui;
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
};
struct FileTreeNode {
    XDOSLVGL *ui;
    lv_obj_t *preview;
    lv_obj_t *children;
    char path[512];
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
    FIL file;
    if (f_open(&file,node.path,FA_READ)!=FR_OK) {
        lv_textarea_set_text(node.preview,"Unable to open file.");
        return;
    }
    char buffer[4097];
    UINT read=0;
    const auto result=f_read(&file,buffer,sizeof buffer-1,&read);
    const bool more=f_size(&file)>read;
    f_close(&file);
    if (result!=FR_OK) {
        lv_textarea_set_text(node.preview,"Unable to read file.");
        return;
    }
    for (UINT i=0;i<read;++i) {
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
}

const char *XDOSLVGL::TakeFileTreeLaunch()
{
    if (!fileTreeLaunchPending_) return nullptr;
    fileTreeLaunchPending_=false;
    return fileTreeLaunch_;
}

void XDOSLVGL::AddFileTreeNode(lv_obj_t *parent,lv_obj_t *preview,XDOSLVGL *ui,
                              const char *label,const char *path,bool directory)
{
    auto *browser=static_cast<FileBrowser *>(lv_obj_get_user_data(preview));
    auto *node=new FileTreeNode{ui,preview,nullptr,{},directory,parent!=browser->list};
    if (!node) return;
    strcpy(node->path,path);
    auto *group=lv_obj_create(parent);
    StyleTreeColumn(group);
    lv_obj_remove_flag(group,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(group,lv_pct(100),LV_SIZE_CONTENT);
    auto *button=lv_button_create(group);
    StyleWindowSurface(button,0x294A6A,0xFFFFFF,0xB6C4D3,1);
    lv_obj_set_style_pad_all(button,4,0);
    lv_obj_set_size(button,lv_pct(100),27);
    auto *name=lv_label_create(button);
    lv_obj_set_width(name,lv_pct(100));
    lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    lv_label_set_text_fmt(name,"%s %s",directory ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE,label);
    lv_obj_align(name,LV_ALIGN_LEFT_MID,0,0);
    lv_obj_add_event_cb(button,FileTreeClick,LV_EVENT_CLICKED,node);
    lv_obj_add_event_cb(button,[](lv_event_t *event) {
        delete static_cast<FileTreeNode *>(lv_event_get_user_data(event));
    },LV_EVENT_DELETE,node);
}

void XDOSLVGL::FileTreeClick(lv_event_t *event)
{
    auto &node=*static_cast<FileTreeNode *>(lv_event_get_user_data(event));
    if (!node.directory) {
        const size_t size=strlen(node.path);
        if (size>5 && !strcasecmp(node.path+size-5,".XAPP")) {
            if (node.ui->windowClosing_) return;
            strcpy(node.ui->fileTreeLaunch_,node.path);
            node.ui->fileTreeLaunchPending_=true;
            node.ui->CloseCurrentApp();
        } else {
            PreviewTreeFile(node);
        }
        return;
    }
    auto *browser=static_cast<FileBrowser *>(lv_obj_get_user_data(node.preview));
    // Copy before changing the list because navigation deletes its old rows.
    char destination[sizeof node.path];
    strcpy(destination,node.path);
    const bool fromTree=node.tree;
    node.ui->NavigateFiles(browser,destination);
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
    const int result=xdos_directory_visit(node.path,0,XDOS_DIRECTORY_LIMIT-used,1,
        [](void *context,const char *name,int) {
            auto &parent=*static_cast<FileTreeNode *>(context);
            char path[sizeof parent.path];
            const int length=snprintf(path,sizeof path,"%s%s/",parent.path,name);
            if (length>=0 && unsigned(length)<sizeof path)
                AddFileTreeNode(parent.children,parent.preview,parent.ui,name,path,true);
        },&node,&listed,&more);
    if (!listed || more || result!=FR_OK) {
        auto *label=lv_label_create(node.children);
        lv_label_set_text(label,result!=FR_OK ? "Directory read failed." :
                          more ? "More folders in file list." : "(no subfolders)");
    }
}

void XDOSLVGL::RenderFiles(void *context)
{
    auto &browser=*static_cast<FileBrowser *>(context);
    const char *path=browser.history[browser.position];
    lv_label_set_text(browser.address,path);
    lv_obj_clean(browser.list);
    lv_label_set_text(browser.notice,"");
    lv_obj_scroll_to_y(browser.list,0,LV_ANIM_OFF);
    lv_textarea_set_text(browser.preview,"Select a file to preview. Select an .XAPP to launch.");
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
    const int result=xdos_directory_visit(path,browser.page*FilePageSize,FilePageSize,0,
        [](void *context,const char *name,int directory) {
            auto &b=*static_cast<FileBrowser *>(context);
            char child[sizeof b.history[0]];
            const int length=snprintf(child,sizeof child,"%s%s%s",b.history[b.position],
                                      name,directory ? "/" : "");
            if (length>=0 && unsigned(length)<sizeof child)
                AddFileTreeNode(b.list,b.preview,b.ui,name,child,directory);
        },&browser,&entryIndex,&more);
    enabled(browser.nextPage,more);
    if (more || browser.page)
        lv_label_set_text(browser.notice,"Folder exceeds 128 entries. Showing up to 128 per page.");
    if (result!=FR_OK || !entryIndex) {
        lv_textarea_set_text(browser.preview,result!=FR_OK ? "Directory read failed." : "Folder is empty.");
    }
}

void XDOSLVGL::NavigateFiles(void *context,const char *path)
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

void XDOSLVGL::OpenFileTree(lv_obj_t *body)
{
    auto *browser=new FileBrowser{};
    if (!browser) return;
    browser->ui=this;
    lv_obj_add_event_cb(body,[](lv_event_t *event) {
        delete static_cast<FileBrowser *>(lv_event_get_user_data(event));
    },LV_EVENT_DELETE,browser);
    auto *bar=lv_obj_create(body);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar,lv_pct(98),62);
    lv_obj_align(bar,LV_ALIGN_TOP_MID,0,3);

    auto nav=[](lv_event_t *event) {
        auto *b=static_cast<FileBrowser *>(lv_event_get_user_data(event));
        auto *target=lv_event_get_target_obj(event);
        if (target==b->back && b->position) --b->position;
        else if (target==b->forward && b->position+1<b->count) ++b->position;
        else if (target==b->previousPage && b->page) { --b->page; b->ui->RenderFiles(b); return; }
        else if (target==b->nextPage) { ++b->page; b->ui->RenderFiles(b); return; }
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
                b->ui->NavigateFiles(b,parent);
                return;
            }
        }
        b->page=0;
        b->ui->RenderFiles(b);
    };
    const char *labels[]={"Back","Forward","Up","Refresh","Prev","Next"};
    lv_obj_t *buttons[6];
    const int buttonWidth=LV_MIN(106,(desktop_->Width()-45)/6);
    for (int i=0;i<6;++i) {
        buttons[i]=MakeWindowButton(bar,labels[i],nav,browser);
        lv_obj_set_size(buttons[i],buttonWidth-4,27);
        lv_obj_align(buttons[i],LV_ALIGN_TOP_LEFT,i*buttonWidth,0);
    }
    browser->back=buttons[0];
    browser->forward=buttons[1];
    browser->previousPage=buttons[4];
    browser->nextPage=buttons[5];
    browser->address=lv_label_create(bar);
    lv_obj_set_width(browser->address,lv_pct(100));
    lv_label_set_long_mode(browser->address,LV_LABEL_LONG_DOT);
    lv_obj_align(browser->address,LV_ALIGN_TOP_LEFT,4,34);

    lv_obj_update_layout(body);
    const int paneHeight=LV_MAX(100,lv_obj_get_height(body)-92);
    auto *tree=browser->tree=lv_obj_create(body);
    StyleTreeColumn(tree);
    lv_obj_set_style_bg_color(tree,lv_color_hex(0xE7EFF5),0);
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
    AddFileTreeNode(tree,preview,this,"SD Card","SD:/",true);
    AddFileTreeNode(tree,preview,this,"USB Drive","USB:/",true);
    NavigateFiles(browser,"SD:/");
}