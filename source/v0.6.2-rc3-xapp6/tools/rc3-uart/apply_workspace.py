"""Install the Paint surface and shared service workspace for Files, Browser and Update."""

from pathlib import Path
import re
import shutil
import sys

src = Path(sys.argv[1])


def edit(file, before, after, count=1):
    p = src / file
    s = p.read_text(encoding="utf-8")
    if s.count(before) != count:
        raise RuntimeError(f"Unexpected source layout in {file}: {before!r}")
    p.write_text(s.replace(before, after), encoding="utf-8")


def regex(file, before, after):
    p = src / file
    s, n = re.subn(before, lambda _: after, p.read_text(encoding="utf-8"))
    if n != 1:
        raise RuntimeError(f"Unexpected source layout in {file}: {before!r}")
    p.write_text(s, encoding="utf-8")


shutil.copyfile(Path(__file__).with_name("xapp_workspace.h"), src / "xapp_workspace.h")
edit("desktop.h", "    bool TakeNotepadRequest();", """    bool TakeNotepadRequest();
    enum Workspace { NoWorkspace, PaintApp, FilesApp, BrowserApp, UpdateApp };
    Workspace TakeWorkspaceRequest();
    bool RequestList() { return QueueStorage(StorageList); }
    bool RequestRead(const char *name) { return QueueStorage(StorageCat, name); }
    bool RequestHttp(const char *url);
    const char *BrowserText() const { return browserContent_; }
    const char *BrowserMessage() const { return browserStatus_; }
    unsigned BrowserRevision() const { return browserRevision_; }
    const char *FilesText() const { return filesContent_; }
    const char *FilesMessage() const { return filesStatus_; }
    unsigned FilesRevision() const { return filesRevision_; }
    bool BootServiceReady() const { return storageReady_ && !storageBusy_ && !storagePending_; }""")
edit("desktop.h", "    enum { NWIN=6, NPAINT=1800 };", "    enum { NWIN=6 };")
edit("desktop.h", """    struct Mark { unsigned short x, y; };
    Window windows_[NWIN];
    Mark marks_[NPAINT];
    unsigned markCount_;""", """    Window windows_[NWIN];
    Workspace workspacePending_ = NoWorkspace;
    unsigned filesRevision_ = 0, browserRevision_ = 0;
    char filesStatus_[96] = {};""")
for name in ("Paint", "Files", "Browser"):
    edit("desktop.h", f"    void Draw{name}(int i);\n", "")
    regex("desktop.cpp", rf"(?ms)^void Desktop::Draw{name}\([^\n]*\)\n\{{\n.*?^\}}\n", "")
edit("desktop.h", "    void PaintAt(int x,int y);\n", "")
regex("desktop.cpp", r"(?ms)^void Desktop::PaintAt\([^\n]*\)\n\{\n.*?^\}\n", "")
edit("desktop.cpp", "Desktop::Desktop() : markCount_(0),", "Desktop::Desktop() :")
edit("desktop.cpp", "        } else if((buttons&1) && active_==Paint && windows_[Paint].open) PaintAt(x,y);", "        }")
edit("desktop.cpp", "        if(i==Paint) PaintAt(x,y);\n", "")
regex("desktop.cpp", r"(?ms)^        if\(i==Browser && browserLoading_ &&\n.*?^        \}\n", "")
regex("desktop.cpp", r"(?ms)^        if\(i==Browser && !browserLoading_ &&.*?^        \}\n", "")
regex("desktop.cpp", r"(?ms)^    if\(active_==Browser\) \{\n.*?^    \}\n", "")
edit("desktop.cpp", """    case Paint:DrawPaint(i);break;
        case Browser:DrawBrowser(i);break;case Files:DrawFiles(i);break;""", "")
edit("desktop.cpp", "void Desktop::Open(int i)\n{", """Desktop::Workspace Desktop::TakeWorkspaceRequest()
{
    const auto request = workspacePending_;
    workspacePending_ = NoWorkspace;
    return request;
}

void Desktop::Open(int i)
{
    if (i == Paint || i == Files || i == Browser) {
        workspacePending_ = i == Paint ? PaintApp : (i == Files ? FilesApp : BrowserApp);
        return;
    }""")
edit("desktop.cpp", "    if(i==Files && storageReady_ && !storageBusy_)QueueStorage(StorageList);\n", "")
edit("desktop.cpp", "    if(Contains(x,y,151,97+NWIN*80,112,72))", """    if(Contains(x,y,422,97+NWIN*80,137,72)) {workspacePending_=UpdateApp;return;}
    if(Contains(x,y,151,97+NWIN*80,112,72))""")
edit("desktop.cpp", '    Text(151,shortcutY+38,"XDS BENCHMARK",WHITE);', '''    Text(151,shortcutY+38,"XDS BENCHMARK",WHITE);
    Fill(433,shortcutY,34,32,0x4D7E71);
    Frame(433,shortcutY,34,32,0xC0D4EA);
    Text(446,shortcutY+8,"U",WHITE);
    Text(422,shortcutY+38,"XDOS UPDATE",WHITE);''')
edit("desktop.cpp", "void Desktop::BrowserStatus(const char *message)\n{", """bool Desktop::RequestHttp(const char *url)
{
    if (!url || !*url || strlen(url) >= sizeof(browserURL_) || browserLoading_) return false;
    strcpy(browserURL_,url);
    SubmitBrowserURL();
    return true;
}

void Desktop::BrowserStatus(const char *message)
{
    ++browserRevision_;""")
edit("desktop.cpp", """    if((action==StorageList||action==StorageListRoot||action==StorageUSBList) && success && data) {""",
     """    if (action == StorageList || action == StorageListRoot || action == StorageUSBList ||
        action == StorageCat || action == StorageUSBCat) {
        strncpy(filesStatus_,message,sizeof filesStatus_-1);
        filesStatus_[sizeof filesStatus_-1] = 0;
        ++filesRevision_;
        if (action == StorageCat || action == StorageUSBCat) {
            if (success && data) {
                const unsigned n = length < sizeof filesContent_-1 ? length : sizeof filesContent_-1;
                memcpy(filesContent_,data,n);
                filesContent_[n] = 0;
            } else filesContent_[0] = 0;
        }
    }
    if((action==StorageList||action==StorageListRoot||action==StorageUSBList) && success && data) {""")

edit("app_shortcut.h", "const char *XdosRunNotepadShortcut", """const char *XdosRunWorkspaceShortcut(unsigned app, const XdosHostApiV1 *api);
const char *XdosRunNotepadShortcut""")
edit("app_shortcut.cpp", "const char *XdosRunNotepadShortcut(", """const char *XdosRunWorkspaceShortcut(unsigned app, const XdosHostApiV1 *api)
{
    struct App { const char *manifest; const char *module; const char *started; };
    static const App entries[] = {
        {"SD:/XDOS/APPS/PAINT.APP", "SD:/XDOS/APPS/PAINT.XAPP", "XAPP: Paint started"},
        {"SD:/XDOS/APPS/FILES.APP", "SD:/XDOS/APPS/FILES.XAPP", "XAPP: Files started"},
        {"SD:/XDOS/APPS/BROWSER.APP", "SD:/XDOS/APPS/BROWSER.XAPP", "XAPP: Browser started"},
        {"SD:/XDOS/APPS/UPDATE.APP", "SD:/XDOS/APPS/UPDATE.XAPP", "XAPP: Update started"}
    };
    if (!app || app > sizeof entries / sizeof entries[0]) return "XAPP: invalid app";
    const auto &entry = entries[app-1];
    if (!VerifyManifest(entry.manifest,entry.module)) return "XAPP: manifest missing or invalid";
    return XappLoaderV1::ExecuteFile(entry.module,api) == XappLoaderV1::OK ?
           entry.started : "XAPP: module missing or failed to start";
}

const char *XdosRunNotepadShortcut(""")

edit("xdos_lvgl.h", '#include "xapp_editor.h"', '#include "xapp_workspace.h"')
edit("xdos_lvgl.h", "    bool AtDesktop() const;", """    bool AtDesktop() const;
    int32_t OpenCanvas(const char *title, XdosStroke stroke);
    int32_t OpenWorkspace(int panel, const char *title, const char *hint, const char *buttons,
                          XdosWorkspaceAction action, XdosWorkspaceRequest request, void *context);""")
edit("xdos_lvgl.h", "    static void EditorButton(lv_event_t *event);", """    static void EditorButton(lv_event_t *event);
    static void PaintPointer(lv_event_t *event);
    static void PaintClear(lv_event_t *event);
    static void WorkspaceButton(lv_event_t *event);
    void PaintEvent(int event, int x, int y);
    void WorkspaceCommand(unsigned index);
    void UpdateWorkspace();
    lv_obj_t *paintCanvas_ = nullptr;
    XdosSurface surface_{};
    XdosStroke stroke_ = nullptr;
    alignas(uint32_t) unsigned char strokeState_[32]{};
    XdosWorkspaceAction workspaceAction_ = nullptr;
    XdosWorkspaceRequest workspaceRequest_ = nullptr;
    void *workspaceContext_ = nullptr;
    lv_obj_t *workspaceInput_ = nullptr, *workspaceOutput_ = nullptr, *workspaceStatus_ = nullptr;
    lv_obj_t *workspaceButtons_[3]{};
    int workspacePanel_ = 0;
    unsigned workspaceRevision_ = 0;""")
edit("xdos_lvgl.cpp", "    ui->editorAction_ = nullptr;", """    if (ui->workspacePanel_ == XDOS_PANEL_HTTP) ui->desktop_->CancelBrowser();
    ui->workspaceAction_ = nullptr;
    ui->workspacePanel_ = 0;
    ui->stroke_ = nullptr;
    ui->paintCanvas_ = nullptr;
    ui->editorAction_ = nullptr;""")
edit("xdos_lvgl.cpp", "    if (editorAction_) UpdateEditor();", """    if (editorAction_) UpdateEditor();
    if (workspaceAction_) UpdateWorkspace();""")
edit("xdos_lvgl.cpp", "    if (!keypadInput_) return !AtDesktop();", """    if (workspaceAction_) {
        const unsigned key = static_cast<unsigned char>(ch);
        if (key == 8 || key == 127) lv_textarea_delete_char(workspaceInput_);
        else if ((key == 10 || key == 13) && workspacePanel_ != XDOS_PANEL_UPDATE)
            WorkspaceCommand(workspacePanel_ == XDOS_PANEL_FILES ? 2 : 0);
        else if (key >= 32 && key <= 126) lv_textarea_add_char(workspaceInput_,key);
        return true;
    }
    if (!keypadInput_) return !AtDesktop();""")
edit("xdos_lvgl.cpp", "void XDOSLVGL::Refresh(unsigned)", """void XDOSLVGL::PaintEvent(int event, int x, int y)
{
    if (stroke_ && stroke_(strokeState_,&surface_,event,x,y))
        lv_obj_invalidate(paintCanvas_);
}

void XDOSLVGL::PaintPointer(lv_event_t *event)
{
    auto *ui = static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ui->PaintEvent(XDOS_PAINT_UP,0,0);
        return;
    }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;
    auto *input = lv_indev_active();
    if (!input || !ui->paintCanvas_) return;
    lv_point_t point;
    lv_area_t bounds;
    lv_indev_get_point(input,&point);
    lv_obj_get_coords(ui->paintCanvas_,&bounds);
    ui->PaintEvent(code == LV_EVENT_PRESSED ? XDOS_PAINT_DOWN : XDOS_PAINT_MOVE,
                   point.x-bounds.x1,point.y-bounds.y1);
}

void XDOSLVGL::PaintClear(lv_event_t *event)
{
    static_cast<XDOSLVGL *>(lv_event_get_user_data(event))->PaintEvent(XDOS_PAINT_CLEAR,0,0);
}

int32_t XDOSLVGL::OpenCanvas(const char *title, XdosStroke stroke)
{
    if (!AtDesktop() || demoScreen_ || !title || !stroke) return -1;
    const int width = desktop_->Width() > 640 ? 608 : desktop_->Width()-32;
    const int height = desktop_->Height() > 530 ? 440 : desktop_->Height()-100;
    if (width <= 0 || height <= 0) return -1;
    const bool fresh = !surface_.pixels;
    if (fresh) {
        auto *pixels = new uint16_t[static_cast<unsigned>(width)*height];
        if (!pixels) return -1;
        surface_ = {pixels,width,height,width};
    }
    demoScreen_ = lv_obj_create(nullptr);
    if (!demoScreen_) {
        if (fresh) { delete[] surface_.pixels; surface_ = {}; }
        return -1;
    }
    SuspendMonitor();
    auto *heading = lv_label_create(demoScreen_);
    lv_label_set_text(heading,title);
    lv_obj_align(heading,LV_ALIGN_TOP_LEFT,12,12);
    auto *clear = MakeButton(demoScreen_,"CLEAR",PaintClear,this);
    lv_obj_set_width(clear,88);
    lv_obj_align(clear,LV_ALIGN_TOP_LEFT,92,5);
    paintCanvas_ = lv_canvas_create(demoScreen_);
    lv_canvas_set_buffer(paintCanvas_,surface_.pixels,surface_.width,surface_.height,LV_COLOR_FORMAT_RGB565);
    lv_obj_align(paintCanvas_,LV_ALIGN_TOP_MID,0,62);
    lv_obj_add_flag(paintCanvas_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(paintCanvas_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(paintCanvas_,PaintPointer,LV_EVENT_ALL,this);
    demoBack_ = MakeButton(demoScreen_,"BACK TO DESKTOP",DemoBack,this);
    lv_obj_set_size(demoBack_,168,37);
    lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    memset(strokeState_,0,sizeof strokeState_);
    stroke_ = stroke;
    if (fresh) PaintEvent(XDOS_PAINT_CLEAR,0,0);
    lv_screen_load(demoScreen_);
    return 0;
}

void XDOSLVGL::WorkspaceButton(lv_event_t *event)
{
    auto *ui = static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    auto *target = lv_event_get_target_obj(event);
    for (unsigned i=0;i<3;++i)
        if (ui->workspaceButtons_[i] == target) { ui->WorkspaceCommand(i); return; }
}

void XDOSLVGL::WorkspaceCommand(unsigned index)
{
    if (!workspaceAction_) return;
    const auto result = workspaceAction_(workspaceRequest_,workspaceContext_,index,
                                        lv_textarea_get_text(workspaceInput_));
    lv_label_set_text(workspaceStatus_,result ? "Request rejected. Check input or device status." :
        (workspacePanel_ == XDOS_PANEL_UPDATE ? "Done. Restart to apply boot changes." : "Request accepted"));
}

void XDOSLVGL::UpdateWorkspace()
{
    if (workspacePanel_ == XDOS_PANEL_UPDATE) return;
    const bool files = workspacePanel_ == XDOS_PANEL_FILES;
    const auto revision = files ? desktop_->FilesRevision() : desktop_->BrowserRevision();
    if (revision == workspaceRevision_) return;
    workspaceRevision_ = revision;
    lv_label_set_text(workspaceStatus_, files ? desktop_->FilesMessage() : desktop_->BrowserMessage());
    lv_textarea_set_text(workspaceOutput_, files ? desktop_->FilesText() : desktop_->BrowserText());
    lv_textarea_set_cursor_pos(workspaceOutput_,0);
}

int32_t XDOSLVGL::OpenWorkspace(int panel, const char *title, const char *hint, const char *buttons,
                                XdosWorkspaceAction action, XdosWorkspaceRequest request, void *context)
{
    if (!AtDesktop() || demoScreen_ || panel < XDOS_PANEL_FILES || panel > XDOS_PANEL_UPDATE ||
        !title || !hint || !buttons || !action || !request) return -1;
    demoScreen_ = lv_obj_create(nullptr);
    if (!demoScreen_) return -1;
    SuspendMonitor();
    auto *heading = lv_label_create(demoScreen_);
    lv_label_set_text(heading,title);
    lv_obj_align(heading,LV_ALIGN_TOP_LEFT,12,12);
    workspaceInput_ = lv_textarea_create(demoScreen_);
    lv_textarea_set_one_line(workspaceInput_,true);
    lv_textarea_set_max_length(workspaceInput_,191);
    lv_textarea_set_placeholder_text(workspaceInput_,hint);
    lv_obj_set_width(workspaceInput_,lv_pct(94));
    lv_obj_align(workspaceInput_,LV_ALIGN_TOP_MID,0,48);
    memset(workspaceButtons_,0,sizeof workspaceButtons_);
    for (unsigned i=0; i<3 && *buttons; ++i) {
        char label[24];
        unsigned n=0;
        while (*buttons && *buttons!='|') {
            if (n+1 < sizeof label) label[n++]=*buttons;
            ++buttons;
        }
        label[n]=0;
        if (*buttons=='|') ++buttons;
        auto *button = MakeButton(demoScreen_,label,WorkspaceButton,this);
        workspaceButtons_[i]=button;
        lv_obj_set_width(button,138);
        lv_obj_align(button,LV_ALIGN_TOP_LEFT,12+146*i,94);
    }
    workspaceStatus_ = lv_label_create(demoScreen_);
    lv_obj_align(workspaceStatus_,LV_ALIGN_TOP_LEFT,12,142);
    workspaceOutput_ = lv_textarea_create(demoScreen_);
    lv_obj_set_size(workspaceOutput_,lv_pct(94),lv_pct(68));
    lv_obj_align(workspaceOutput_,LV_ALIGN_TOP_MID,0,168);
    lv_textarea_set_cursor_click_pos(workspaceOutput_,false);
    demoBack_ = MakeButton(demoScreen_,"BACK TO DESKTOP",DemoBack,this);
    lv_obj_set_size(demoBack_,168,37);
    lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    workspaceAction_=action;
    workspaceRequest_=request;
    workspaceContext_=context;
    workspacePanel_=panel;
    workspaceRevision_=~0U;
    lv_screen_load(demoScreen_);
    if (panel == XDOS_PANEL_FILES) WorkspaceCommand(0);
    if (panel == XDOS_PANEL_UPDATE) {
        lv_textarea_set_text(workspaceOutput_,
            "Stage an image using UART scp. Select it with NAME.IMG <CRC32>.\\n"
            "Restore Boot reloads the previous config.txt. Type RESET to confirm a restart.");
    }
    return 0;
}

void XDOSLVGL::Refresh(unsigned)""")

edit("kernel.cpp", "        const bool openWidgets = desktop_.TakeAppLaunchRequest();", """#if XDOS_USE_LVGL
        const auto workspace = desktop_.TakeWorkspaceRequest();
        if (workspace != Desktop::NoWorkspace && ui_.AtDesktop()) {
            XdosWorkspaceApi apiExt = {{{api, nullptr}, nullptr},
                [](void *ctx, const char *title, XdosStroke stroke) -> int32_t {
                    return static_cast<CKernel *>(ctx)->ui_.OpenCanvas(title,stroke);
                },
                [](void *ctx, int panel, const char *title, const char *hint, const char *buttons,
                        XdosWorkspaceAction action) -> int32_t {
                    return static_cast<CKernel *>(ctx)->ui_.OpenWorkspace(panel,title,hint,buttons,action,
                        [](void *opaque, int op, const char *text) -> int32_t {
                            auto *k = static_cast<CKernel *>(opaque);
                            switch (op) {
                            case XDOS_REQUEST_LIST: return k->desktop_.RequestList() ? 0 : -1;
                            case XDOS_REQUEST_LIST_USB: return k->desktop_.ListUSB() ? 0 : -1;
                            case XDOS_REQUEST_READ: return k->desktop_.RequestRead(text) ? 0 : -1;
                            case XDOS_REQUEST_HTTP: return k->desktop_.RequestHttp(text) ? 0 : -1;
                            case XDOS_REQUEST_CANCEL_HTTP: k->desktop_.CancelBrowser(); return 0;
                            case XDOS_REQUEST_SELECT_IMAGE:
                                return !uart.TransferActive() && k->desktop_.BootServiceReady() &&
                                       XdosUartService::SelectStagedImage(text) ? 0 : -1;
                            case XDOS_REQUEST_RESTORE_BOOT:
                                return !uart.TransferActive() && k->desktop_.BootServiceReady() &&
                                       XdosUartService::RestorePreviousBoot() ? 0 : -1;
                            case XDOS_REQUEST_RESET:
                                if (uart.TransferActive() || !k->desktop_.BootServiceReady()) return -1;
                                XdosUartService::RestartSystem(); return 0;
                            default: return -1;
                            }
                        },ctx);
                }};
            apiExt.editor.keypad.base.struct_bytes = sizeof apiExt;
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunWorkspaceShortcut(workspace,&apiExt.editor.keypad.base) : "XAPP: FAT32 NOT READY");
        }
#endif
        const bool openWidgets = desktop_.TakeAppLaunchRequest();""")
