"""Expose the editor UI and the existing asynchronous NOTE.TXT storage jobs."""

from pathlib import Path
import re
import shutil
import sys


source = Path(sys.argv[1])


def replace(file, before, after):
    path = source / file
    content = path.read_text(encoding="utf-8")
    if content.count(before) != 1:
        raise RuntimeError(f"Unexpected source layout in {path}: {before!r}")
    path.write_text(content.replace(before, after), encoding="utf-8")


shutil.copyfile(Path(__file__).with_name("xapp_editor.h"), source / "xapp_editor.h")
replace("desktop.h", "    bool TakeCalculatorRequest();",
        "    bool TakeCalculatorRequest();\n    bool TakeNotepadRequest();")
replace("desktop.h", "    bool calculatorPending_ = false;",
        "    bool calculatorPending_ = false;\n    bool notepadPending_ = false;\n    unsigned noteStorageRevision_ = 0;")
replace("desktop.h", "    unsigned NoteRevision() const { return noteRevision_; }", """    unsigned NoteRevision() const { return noteRevision_; }
    unsigned NoteStorageRevision() const { return noteStorageRevision_; }
    const char *NoteStatus() const { return noteStatus_; }
    bool SaveNoteText(const char *text) {
        if (!storageReady_ || storageBusy_ || storagePending_) return false;
        SetNoteFromUI(text);
        return SaveNote();
    }""")
replace("desktop.h", "    void DrawNotepad(int i);\n", "")
replace("desktop.cpp", "void Desktop::Open(int i)\n{", """bool Desktop::TakeNotepadRequest()
{
    const bool pending = notepadPending_;
    notepadPending_ = false;
    return pending;
}

void Desktop::Open(int i)
{
    if (i == Notepad) {
        notepadPending_ = true;
        return;
    }""")
replace("desktop.cpp", """        if(i==Notepad) {
            if(Contains(x,y,w.x+w.w-162,w.y+45,62,27)) {QueueStorage(StorageSaveNote);return;}
            if(Contains(x,y,w.x+w.w-92,w.y+45,62,27)) {QueueStorage(StorageLoadNote);return;}
        }
""", "")
replace("desktop.cpp", "    switch(i){case Terminal:DrawTerminal(i);break;case Notepad:DrawNotepad(i);break;",
        "    switch(i){case Terminal:DrawTerminal(i);break;")
replace("desktop.cpp", "    if(action==StorageSaveNote || action==StorageLoadNote) {",
        "    if(action==StorageSaveNote || action==StorageLoadNote) {\n        ++noteStorageRevision_;")
path = source / "desktop.cpp"
content, count = re.subn(r"(?ms)^void Desktop::DrawNotepad\([^\n]*\)\n\{\n.*?^\}\n", "",
                         path.read_text(encoding="utf-8"))
if count != 1:
    raise RuntimeError("Cannot remove original Notepad renderer")
content, count = re.subn(r"(?ms)^    if\(active_==Notepad\) \{\n.*?^    \}\n", "", content)
if count != 1:
    raise RuntimeError("Cannot remove original Notepad input")
path.write_text(content, encoding="utf-8")

replace("app_shortcut.h", "const char *XdosRunCalculatorShortcut",
        "const char *XdosRunNotepadShortcut(const XdosHostApiV1 *api);\nconst char *XdosRunCalculatorShortcut")
replace("app_shortcut.cpp", "const char *XdosRunCalculatorShortcut(", """const char *XdosRunNotepadShortcut(const XdosHostApiV1 *api)
{
    static const char manifest[] = "SD:/XDOS/APPS/NOTE.APP";
    static const char module[] = "SD:/XDOS/APPS/NOTE.XAPP";
    if (!VerifyManifest(manifest, module)) return "XAPP: NOTE.APP missing or invalid";
    const auto result = XappLoaderV1::ExecuteFile(module, api);
    if (result == XappLoaderV1::OK) return "XAPP: NOTE.XAPP started";
    if (result == XappLoaderV1::OPEN_FAILED) return "XAPP: NOTE.XAPP missing";
    return "XAPP: Notepad load or launch failed";
}

const char *XdosRunCalculatorShortcut(""")

replace("xdos_lvgl.h", '#include "xapp_keypad.h"', '#include "xapp_editor.h"')
replace("xdos_lvgl.h", "    bool AtDesktop() const;", """    bool AtDesktop() const;
    int32_t OpenEditor(const char *title, XdosEditorAction action);""")
replace("xdos_lvgl.h", "    XdosKeypadInput keypadInput_ = nullptr;", """    static void EditorButton(lv_event_t *event);
    void EditorCommand(int action);
    void UpdateEditor();
    XdosEditorAction editorAction_ = nullptr;
    lv_obj_t *editorText_ = nullptr;
    lv_obj_t *editorStatus_ = nullptr;
    unsigned editorStorageRevision_ = 0, editorNoteRevision_ = 0;
    int editorPending_ = 0;
    XdosKeypadInput keypadInput_ = nullptr;""")
replace("xdos_lvgl.cpp", "    ui->keypadInput_ = nullptr;", """    ui->editorAction_ = nullptr;
    if (ui->editorText_ && ui->editorPending_ != XDOS_EDITOR_LOAD)
        ui->desktop_->SetNoteFromUI(lv_textarea_get_text(ui->editorText_));
    ui->editorPending_ = 0;
    ui->editorText_ = ui->editorStatus_ = nullptr;
    ui->keypadInput_ = nullptr;""")
replace("xdos_lvgl.cpp", "void XDOSLVGL::Tick(unsigned nowUs) {benchmark_.Tick(nowUs);}", """void XDOSLVGL::Tick(unsigned nowUs)
{
    benchmark_.Tick(nowUs);
    if (editorAction_) UpdateEditor();
}""")
replace("xdos_lvgl.cpp", "    if (!keypadInput_) return !AtDesktop();", """    if (editorAction_) {
        const unsigned key = static_cast<unsigned char>(ch);
        if (key == 19) EditorCommand(XDOS_EDITOR_SAVE);
        else if (key == 15) EditorCommand(XDOS_EDITOR_LOAD);
        else if (editorPending_ != XDOS_EDITOR_LOAD) {
            if (key == 8 || key == 127) lv_textarea_delete_char(editorText_);
            else if (strlen(lv_textarea_get_text(editorText_)) < 2047) {
                if (key == 10 || key == 13) lv_textarea_add_char(editorText_,'\\n');
                else if (key >= 32 && key <= 126) lv_textarea_add_char(editorText_,key);
            }
        }
        return true;
    }
    if (!keypadInput_) return !AtDesktop();""")
replace("xdos_lvgl.cpp", "void XDOSLVGL::Refresh(unsigned)", """void XDOSLVGL::EditorButton(lv_event_t *event)
{
    auto *ui = static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    auto *label = lv_obj_get_child(lv_event_get_target_obj(event),0);
    if (label)
        ui->EditorCommand(lv_label_get_text(label)[0] == 'S' ? XDOS_EDITOR_SAVE : XDOS_EDITOR_LOAD);
}

void XDOSLVGL::EditorCommand(int action)
{
    if (!editorAction_ || editorPending_) return;
    const char *text = lv_textarea_get_text(editorText_);
    editorStorageRevision_ = desktop_->NoteStorageRevision();
    editorNoteRevision_ = desktop_->NoteRevision();
    const auto result = editorAction_([](void *ctx, int action, const char *text) -> int32_t {
        auto *desktop = static_cast<Desktop *>(ctx);
        if (action == XDOS_EDITOR_LOAD) return desktop->LoadNote() ? 0 : -1;
        if (action == XDOS_EDITOR_SAVE && text && strlen(text) <= 2047)
            return desktop->SaveNoteText(text) ? 0 : -1;
        return -1;
    },desktop_,action,text);
    if (result == 0) {
        editorPending_ = action;
        lv_label_set_text(editorStatus_,action == XDOS_EDITOR_LOAD ? "Loading..." : "Saving...");
    } else {
        lv_label_set_text(editorStatus_, "Request rejected: SD unavailable, busy, or note too long");
    }
}

void XDOSLVGL::UpdateEditor()
{
    if (!editorPending_ || editorStorageRevision_ == desktop_->NoteStorageRevision()) return;
    if (editorPending_ == XDOS_EDITOR_LOAD && editorNoteRevision_ != desktop_->NoteRevision())
        lv_textarea_set_text(editorText_,desktop_->NoteText());
    lv_label_set_text(editorStatus_,desktop_->NoteStatus());
    editorPending_ = 0;
}

int32_t XDOSLVGL::OpenEditor(const char *title, XdosEditorAction action)
{
    if (!AtDesktop() || demoScreen_ || !title || !action) return -1;
    demoScreen_ = lv_obj_create(nullptr);
    if (!demoScreen_) return -1;
    SuspendMonitor();
    auto *heading = lv_label_create(demoScreen_);
    lv_label_set_text(heading,title);
    lv_obj_align(heading,LV_ALIGN_TOP_LEFT,12,12);
    auto *save = MakeButton(demoScreen_,"SAVE",EditorButton,this);
    lv_obj_set_width(save,82);
    lv_obj_align(save,LV_ALIGN_TOP_LEFT,12,42);
    auto *load = MakeButton(demoScreen_,"LOAD",EditorButton,this);
    lv_obj_set_width(load,82);
    lv_obj_align(load,LV_ALIGN_TOP_LEFT,102,42);
    editorText_ = lv_textarea_create(demoScreen_);
    lv_obj_set_size(editorText_,lv_pct(94),lv_pct(75));
    lv_obj_align(editorText_,LV_ALIGN_TOP_MID,0,90);
    lv_textarea_set_max_length(editorText_,2047);
    lv_textarea_set_text(editorText_,desktop_->NoteText());
    editorStatus_ = lv_label_create(demoScreen_);
    lv_obj_align(editorStatus_,LV_ALIGN_BOTTOM_LEFT,12,-8);
    lv_label_set_text(editorStatus_,"Ctrl+S: Save   Ctrl+O: Load");
    demoBack_ = MakeButton(demoScreen_,"BACK TO DESKTOP",DemoBack,this);
    lv_obj_set_size(demoBack_,168,37);
    lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    editorAction_ = action;
    editorPending_ = 0;
    lv_screen_load(demoScreen_);
    return 0;
}

void XDOSLVGL::Refresh(unsigned)""")
replace("xdos_lvgl.cpp", "#include <stdio.h>", "#include <stdio.h>\n#include <string.h>")

replace("kernel.cpp", "        const bool openWidgets = desktop_.TakeAppLaunchRequest();", """#if XDOS_USE_LVGL
        if (desktop_.TakeNotepadRequest() && ui_.AtDesktop()) {
            XdosEditorApi editor = {{api, nullptr}, [](void *ctx, const char *title,
                                                    XdosEditorAction action) -> int32_t {
                return static_cast<CKernel *>(ctx)->ui_.OpenEditor(title, action);
            }};
            editor.keypad.base.struct_bytes = sizeof editor;
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunNotepadShortcut(&editor.keypad.base) : "XAPP: FAT32 NOT READY");
        }
#endif
        const bool openWidgets = desktop_.TakeAppLaunchRequest();""")
