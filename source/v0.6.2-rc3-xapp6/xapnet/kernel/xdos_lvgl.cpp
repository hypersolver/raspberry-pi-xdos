#include "xapp_v2_runtime.h"
#include "xdos_file_delete.h"
#include "xdos_lvgl.h"
#include "xdos_text_keys.h"
#include <lvgl/src/misc/lv_timer_private.h>
#include <circle/xdos_test_debug.h>
#include "xdos_lvgl_tail.h"
#include "desktop.h"
#include <fatfs/ff.h>
#include <strings.h>
#include "xdos_arduino/xdos_arduino_runtime.h"
#include <circle/serial.h>
#include <stdio.h>
#include <string.h>

namespace { static const unsigned LOOP_SAMPLE_US=500000U; }
XDOSLVGL::XDOSLVGL()
: desktop_(0),serial_(0),homeScreen_(0),demoScreen_(0),title_(0),status_(0),usbStatus_(0),term_(0),sketch_(0),lastUsbStorageRevision_(~0U),
  lastStatusRevision_(~0U),lastTerminalRevision_(~0U),lastLoops_(~0U),
  lastLoopSampleUs_(0),consoleLines_(16),textUpdateCount_(0)
{ terminalView_[0]=0; }

void XDOSLVGL::USBListButton(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED)return;
    XDOSLVGL *ui=(XDOSLVGL*)lv_event_get_user_data(event);
    if(ui && ui->desktop_)ui->desktop_->ListUSB();
}
void XDOSLVGL::USBEjectButton(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED)return;
    XDOSLVGL *ui=(XDOSLVGL*)lv_event_get_user_data(event);
    if(ui && ui->desktop_)ui->desktop_->EjectUSB();
}
void XDOSLVGL::DemoButton(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    XDOSLVGL *ui=(XDOSLVGL *)lv_event_get_user_data(event);
    if(ui && ui->desktop_) ui->desktop_->RequestLVGL();
}
extern "C" void xdos_scroll_measure_end();
extern "C" void xdos_gpu_scroll_end();

void XDOSLVGL::DemoBack(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    XDOSLVGL *ui=(XDOSLVGL *)lv_event_get_user_data(event);
    if(!ui || !ui->homeScreen_ || !ui->demoScreen_) return;
    if (ui->sketchActive_) {
        xdos_gpu_scroll_end();
        xdos_scroll_measure_end();
        ui->sketchLoop_ = nullptr;
        if (ui->sketchMeasurements_) lv_sysmon_performance_pause(lv_display_get_default());
        if (ui->sketchStop_) ui->sketchStop_();
        ui->sketchStop_ = nullptr;
        ui->sketchActive_ = false;
        ui->sketchMeasurements_ = false;
        auto *screen = ui->demoScreen_;
        ui->demoScreen_ = nullptr;
        lv_obj_delete_async(ui->demoBack_);
        ui->demoBack_ = nullptr;
        lv_screen_load(ui->homeScreen_);
        // App-owned object delete callbacks must run before the slot is reused.
        lv_obj_delete(screen);
        if (XDOS_TEST_DEBUG && ui->serial_) {
            static const char message[] = "[XAPP] Arduino LVGL session closed\r\n";
            ui->serial_->Write(message,sizeof message-1);
        }
        return;
    }
    if (ui->workspacePanel_ == XDOS_PANEL_MUSIC && ui->workspaceRequest_)
        ui->workspaceRequest_(ui->workspaceContext_,XDOS_REQUEST_MUSIC_STOP,nullptr);
    if (ui->workspacePanel_ == XDOS_PANEL_HTTP) ui->desktop_->CancelBrowser();
    ui->workspaceAction_ = nullptr;
    ui->workspacePanel_ = 0;
    ui->stroke_ = nullptr;
    ui->paintCanvas_ = nullptr;
    ui->editorAction_ = nullptr;
    if (ui->editorText_ && ui->editorPending_ != XDOS_EDITOR_LOAD)
        ui->desktop_->SetNoteFromUI(lv_textarea_get_text(ui->editorText_));
    ui->editorPending_ = 0;
    ui->editorText_ = ui->editorStatus_ = nullptr;
    ui->keypadInput_ = nullptr;
    ui->keypadDisplay_ = nullptr;
    if (lv_obj_get_parent(ui->demoScreen_) == ui->homeScreen_) {
        ui->BeginAppWindowClose();
        return;
    }
    lv_obj_delete_async(ui->demoBack_);
    ui->demoBack_=nullptr;
    lv_screen_load(ui->homeScreen_);
    lv_obj_delete_async(ui->demoScreen_);
    ui->demoScreen_=0;
}
static lv_obj_t *MakeButton(lv_obj_t *parent,const char *label,
                             lv_event_cb_t event,void *owner)
{
    lv_obj_t *button=lv_button_create(parent);
    lv_obj_set_height(button,37);
    lv_obj_add_event_cb(button,event,LV_EVENT_CLICKED,owner);
    lv_obj_t *text=lv_label_create(button);
    lv_label_set_text(text,label);
    lv_obj_center(text);
    return button;
}
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
    auto *button = MakeButton(parent, label, event, owner);
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

void DragWindow(lv_event_t *event)
{
    auto *input = lv_indev_active();
    if (!input) return;
    auto *window = static_cast<lv_obj_t *>(lv_event_get_user_data(event));
    lv_point_t movement;
    lv_indev_get_vect(input, &movement);
    const int maxX = lv_obj_get_width(lv_obj_get_parent(window)) - lv_obj_get_width(window);
    const int maxY = lv_obj_get_height(lv_obj_get_parent(window)) - lv_obj_get_height(window);
    const int x = lv_obj_get_x(window) + movement.x;
    const int y = lv_obj_get_y(window) + movement.y;
    lv_obj_set_pos(window, LV_CLAMP(0, x, maxX), LV_CLAMP(0, y, maxY));
}
}

lv_obj_t *XDOSLVGL::OpenAppWindow(const char *title, int width, int height)
{
    const int desktopWidth = desktop_->Width();
    const int desktopHeight = desktop_->Height();
    height=LV_MIN(height+22,desktopHeight-49);
    demoScreen_ = lv_obj_create(homeScreen_);
    if (!demoScreen_) return nullptr;
    auto *window = demoScreen_;
    lv_obj_remove_flag(window, LV_OBJ_FLAG_SCROLLABLE);
    StyleWindowSurface(window, 0xF8FAFD, 0x203047, 0xB6C4D3, 1);
    lv_obj_set_style_pad_all(window, 0, 0);
    lv_obj_set_style_shadow_color(window, lv_color_hex(0x071522), 0);
    lv_obj_set_style_shadow_opa(window, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(window, 8, 0);
    lv_obj_set_style_shadow_offset_y(window, 5, 0);
    // Position from the top left so drag offsets use the same coordinate space.
    lv_obj_set_size(window, width, height);
    lv_obj_set_pos(window, (desktopWidth-width)/2, (desktopHeight-height)/2);

    auto *bar = lv_obj_create(window);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    StyleWindowSurface(bar, 0x294A6A, 0xFFFFFF, 0x4FC7BB, 2);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_size(bar, lv_pct(100), 35);
    lv_obj_add_event_cb(bar, DragWindow, LV_EVENT_PRESSING, window);

    auto *heading = lv_label_create(bar);
    lv_label_set_text(heading, title);
    lv_obj_align(heading, LV_ALIGN_LEFT_MID, 13, 0);
    demoBack_ = MakeWindowButton(bar, "X", DemoBack, this);
    StyleWindowSurface(demoBack_, 0xF27771, 0xFFFFFF, 0xF27771, 0);
    lv_obj_set_style_bg_color(demoBack_, lv_color_hex(0xCC5854), LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(demoBack_, 0, 0);
    lv_obj_set_size(demoBack_, 29, 25);
    lv_obj_align(demoBack_, LV_ALIGN_RIGHT_MID, -6, 0);
    auto *maximize=MakeWindowButton(bar,"[]",AppMaximizePressed,this);
    lv_obj_set_size(maximize,29,25);
    lv_obj_align(maximize,LV_ALIGN_RIGHT_MID,-39,0);
    auto *minimize=MakeWindowButton(bar,"_",AppMinimizePressed,this);
    lv_obj_set_size(minimize,29,25);
    lv_obj_align(minimize,LV_ALIGN_RIGHT_MID,-72,0);

    auto *body = lv_obj_create(window);
    lv_obj_remove_style_all(body);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(body, 0, 35);
    lv_obj_set_size(body, width-2, height-59);
    auto *hint=lv_label_create(window);
    lv_label_set_text(hint,"Alt+F4  Close XAPP");
    lv_obj_set_style_text_color(hint,lv_color_hex(0x294A6A),0);
    lv_obj_align(hint,LV_ALIGN_BOTTOM_LEFT,9,-3);
    ShowAppTask(title);
    appWindowBody_=body;
    if (desktopMonitor_) lv_obj_move_foreground(desktopMonitor_);
    overwriteText_ = false;
    return body;
}

bool XDOSLVGL::CloseCurrentApp()
{
    if (!demoScreen_ || !demoBack_ || windowClosing_) return false;
    lv_obj_send_event(demoBack_, LV_EVENT_CLICKED, nullptr);
    return true;
}

void XDOSLVGL::BeginAppWindowClose()
{
    if (windowClosing_) return;
    windowClosing_ = true;
    auto *window = demoScreen_;
    lv_obj_add_state(window, LV_STATE_DISABLED);
    lv_obj_add_state(demoBack_, LV_STATE_DISABLED);
    FinishAppWindowClose();
}

void XDOSLVGL::FinishAppWindowClose()
{
    ClearAppTask();
    lv_obj_delete_async(demoScreen_);
    demoScreen_ = demoBack_ = nullptr;
    windowClosing_ = false;
    RestoreMonitor();
}

void XDOSLVGL::DesktopLoaded(lv_event_t *event)
{
    static_cast<XDOSLVGL *>(lv_event_get_user_data(event))->RestoreMonitor();
}

void XDOSLVGL::SuspendMonitor()
{
    if (desktopMonitor_) lv_obj_add_flag(desktopMonitor_, LV_OBJ_FLAG_HIDDEN);
}

void XDOSLVGL::RestoreMonitor()
{
    if (desktopMonitor_) lv_obj_remove_flag(desktopMonitor_, LV_OBJ_FLAG_HIDDEN);
}

extern "C" unsigned xdos_secondary_load(unsigned core);

void XDOSLVGL::UpdateMonitor(unsigned cpuPercent)
{
    // CPU0-3 load is drawn in the desktop's USB / NETWORK panel (top right).
    const unsigned cpu1=xdos_secondary_load(1);
    const unsigned cpu2=xdos_secondary_load(2);
    const unsigned cpu3=xdos_secondary_load(3);
    if (desktop_) desktop_->SetCpuLoad(cpuPercent,cpu1,cpu2,cpu3);
    lastCpuPercent_=cpuPercent;
}

void XDOSLVGL::DesktopPointer(lv_event_t *event)
{
    auto *ui = static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    auto *input = lv_indev_active();
    if (!input || !ui->AtDesktop()) return;
    lv_point_t pt;
    lv_indev_get_point(input, &pt);
    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED: ui->desktop_->Mouse(Desktop::Down,1,pt.x,pt.y); break;
    case LV_EVENT_PRESSING: ui->desktop_->Mouse(Desktop::Move,1,pt.x,pt.y); break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: ui->desktop_->Mouse(Desktop::Up,0,pt.x,pt.y); break;
    default: break;
    }
}

void XDOSLVGL::ShowDesktop()
{
    if (!desktopCanvas_) {
        const int w = desktop_->Width(), h = desktop_->Height();
        desktopPixels_ = new uint16_t[static_cast<unsigned>(w) * h];
        if (!desktopPixels_) {
            desktop_->ReportAppResult("DESKTOP BUFFER UNAVAILABLE");
            return;
        }
        lv_obj_set_style_pad_all(homeScreen_,0,0);
        lv_obj_remove_flag(homeScreen_,LV_OBJ_FLAG_SCROLLABLE);
        desktopCanvas_ = lv_canvas_create(homeScreen_);
        lv_canvas_set_buffer(desktopCanvas_,desktopPixels_,w,h,LV_COLOR_FORMAT_RGB565);
        lv_obj_add_flag(desktopCanvas_,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(desktopCanvas_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(desktopCanvas_,DesktopPointer,LV_EVENT_ALL,this);
#if XDOS_LVGL_PERF
        // CPU load label moved into the desktop status panel (apply_cpu_panel.py).
        lv_obj_add_event_cb(homeScreen_,DesktopLoaded,LV_EVENT_SCREEN_LOADED,this);
#endif
    }
    desktop_->RequestFullRepaint();
    Refresh(0);
}

void XDOSLVGL::Initialize(Desktop *desktop,CSerialDevice *serial)
{
    desktop_=desktop;serial_=serial;
    homeScreen_=lv_screen_active();
    lv_sysmon_hide_performance(lv_display_get_default());
    lv_sysmon_performance_pause(lv_display_get_default());
    ShowDesktop();
}

void XDOSLVGL::Tick()
{
    if (editorAction_) UpdateEditor();
    if (workspaceAction_) UpdateWorkspace();
    if (sketchLoop_) sketchLoop_();
}
bool XDOSLVGL::AtDesktop() const
{
    return !demoScreen_ && homeScreen_ && lv_screen_active() == homeScreen_;
}

namespace {
unsigned LineEdge(lv_obj_t *field, bool end)
{
    const unsigned cursor = lv_textarea_get_cursor_pos(field);
    unsigned position = 0, start = 0;
    for (auto *text = lv_textarea_get_text(field); *text; ++text) {
        if ((static_cast<unsigned char>(*text) & 0xC0) == 0x80) continue;
        if (*text == '\n') {
            if (position >= cursor) return end ? position : start;
            start = position + 1;
        }
        ++position;
    }
    return end ? position : start;
}
}

void XDOSLVGL::EditText(lv_obj_t *field, unsigned key)
{
    switch (key) {
    case XdosKeyLeft: lv_textarea_cursor_left(field); return;
    case XdosKeyRight: lv_textarea_cursor_right(field); return;
    case XdosKeyUp: lv_textarea_cursor_up(field); return;
    case XdosKeyDown: lv_textarea_cursor_down(field); return;
    case XdosKeyHome:
    case XdosKeyEnd: lv_textarea_set_cursor_pos(field,LineEdge(field,key == XdosKeyEnd)); return;
    case XdosKeyDelete: lv_textarea_delete_char_forward(field); return;
    case XdosKeyInsert: overwriteText_ = !overwriteText_; return;
    case 8:
    case 127: lv_textarea_delete_char(field); return;
    default: break;
    }
    if (key < 32 || key > 126) return;
    if (overwriteText_ && lv_textarea_get_cursor_pos(field) < LineEdge(field,true))
        lv_textarea_delete_char_forward(field);
    lv_textarea_add_char(field,key);
}

bool XDOSLVGL::OnKey(unsigned key)
{
    if (editorAction_) {
        if (key == 19) EditorCommand(XDOS_EDITOR_SAVE);
        else if (key == 15) EditorCommand(XDOS_EDITOR_LOAD);
        else if (editorPending_ != XDOS_EDITOR_LOAD) {
            if (key == 10 || key == 13) lv_textarea_add_char(editorText_,'\n');
            else EditText(editorText_,key);
        }
        return true;
    }
    if (workspaceAction_) {
        if (key == 10 || key == 13)
            WorkspaceCommand(workspacePanel_ == XDOS_PANEL_FILES ? 2 : 0);
        else EditText(workspaceInput_,key);
        return true;
    }
    if (!keypadInput_) return !AtDesktop();
    if (key < 256) KeypadKey(key);
    return true;
}

void XDOSLVGL::KeypadKey(int key)
{
    if (!keypadInput_) return;
    keypadInput_(keypadState_, key, keypadText_);
    keypadText_[sizeof keypadText_ - 1] = 0;
    lv_label_set_text(keypadDisplay_, keypadText_);
}

void XDOSLVGL::KeypadButton(lv_event_t *event)
{
    auto *ui = static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    auto *label = lv_obj_get_child(lv_event_get_target_obj(event), 0);
    if (label) ui->KeypadKey(lv_label_get_text(label)[0]);
}

int32_t XDOSLVGL::OpenKeypad(const char *title, const char *keys, XdosKeypadInput input)
{
    if (!AtDesktop() || demoScreen_ || !title || !keys || !input) return -1;
    const int width = LV_MIN(362, desktop_->Width()-20);
    const int height = LV_MIN(365, desktop_->Height()-24);
    if (width < 130 || height < 140) return -1;
    auto *panel = OpenAppWindow(title,width,height);
    if (!panel) return -1;
    const int column = (width-22)/4;
    const int row = (height-97)/4;
    keypadDisplay_ = lv_label_create(panel);
    lv_obj_align(keypadDisplay_,LV_ALIGN_TOP_RIGHT,-10,12);
    for (unsigned i = 0; i < 16; ++i) {
        const char label[] = {keys[i],0};
        auto *key = MakeWindowButton(panel,label,KeypadButton,this);
        lv_obj_set_size(key,column-5,row-5);
        lv_obj_set_pos(key,10+(i%4)*column,49+(i/4)*row);
    }
    for (auto &byte : keypadState_) byte = 0;
    keypadText_[0] = 0;
    keypadInput_ = input;
    KeypadKey(0);
    return 0;
}
void XDOSLVGL::EditorButton(lv_event_t *event)
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
    const int width = LV_MIN(710, desktop_->Width()-24);
    const int height = LV_MIN(525, desktop_->Height()-64);
    if (width < 200 || height < 180) return -1;
    auto *panel = OpenAppWindow(title,width,height);
    if (!panel) return -1;
    auto *save = MakeWindowButton(panel,"SAVE",EditorButton,this);
    lv_obj_set_width(save,82);
    lv_obj_align(save,LV_ALIGN_TOP_LEFT,12,8);
    auto *load = MakeWindowButton(panel,"LOAD",EditorButton,this);
    lv_obj_set_width(load,82);
    lv_obj_align(load,LV_ALIGN_TOP_LEFT,102,8);
    editorText_ = MakeWindowTextArea(panel);
    lv_obj_set_size(editorText_,width-26,height-125);
    lv_obj_align(editorText_,LV_ALIGN_TOP_MID,0,53);
    lv_textarea_set_max_length(editorText_,2047);
    lv_textarea_set_text(editorText_,desktop_->NoteText());
    editorStatus_ = lv_label_create(panel);
    lv_obj_align(editorStatus_,LV_ALIGN_BOTTOM_LEFT,12,-8);
    lv_label_set_text(editorStatus_,"Ctrl+S: Save   Ctrl+O: Load");
    editorAction_ = action;
    editorPending_ = 0;
    return 0;
}

void XDOSLVGL::PaintEvent(int event, int x, int y)
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
    const int width = LV_MIN(608, desktop_->Width()-46) & ~1;
    const int height = LV_MIN(440, desktop_->Height()-146);
    if (width <= 0 || height <= 0) return -1;
    const bool fresh = !surface_.pixels;
    if (fresh) {
        auto *pixels = new uint16_t[static_cast<unsigned>(width)*height];
        if (!pixels) return -1;
        surface_ = {pixels,width,height,width};
    }
    auto *panel = OpenAppWindow(title,width+22,height+82);
    if (!panel) {
        if (fresh) { delete[] surface_.pixels; surface_ = {}; }
        return -1;
    }
    auto *clear = MakeWindowButton(panel,"CLEAR",PaintClear,this);
    lv_obj_set_width(clear,88);
    lv_obj_align(clear,LV_ALIGN_TOP_LEFT,10,4);
    paintCanvas_ = lv_canvas_create(panel);
    lv_canvas_set_buffer(paintCanvas_,surface_.pixels,surface_.width,surface_.height,LV_COLOR_FORMAT_RGB565);
    lv_obj_align(paintCanvas_,LV_ALIGN_TOP_MID,0,42);
    lv_obj_add_flag(paintCanvas_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(paintCanvas_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(paintCanvas_,PaintPointer,LV_EVENT_ALL,this);
    memset(strokeState_,0,sizeof strokeState_);
    stroke_ = stroke;
    if (fresh) PaintEvent(XDOS_PAINT_CLEAR,0,0);
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
    if (!result && workspacePanel_==XDOS_PANEL_TERMINAL) lv_textarea_set_text(workspaceInput_,"");
    lv_label_set_text(workspaceStatus_,result ? "Request rejected. Check input or device status." :
        "Request accepted");
}

void XDOSLVGL::UpdateWorkspace()
{
    if (workspacePanel_ == XDOS_PANEL_PLAYER || workspacePanel_ == XDOS_PANEL_MUSIC) return;
    if (workspacePanel_ == XDOS_PANEL_TERMINAL) {
        const auto revision=desktop_->TerminalRevision();
        if (revision!=workspaceRevision_) {
            workspaceRevision_=revision;
            lv_textarea_set_text(workspaceOutput_,desktop_->TerminalText());
            lv_textarea_set_cursor_pos(workspaceOutput_,LV_TEXTAREA_CURSOR_LAST);
        }
        return;
    }
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
    if (!AtDesktop() || demoScreen_ || panel < XDOS_PANEL_FILES || (panel > XDOS_PANEL_HTTP && panel != XDOS_PANEL_TERMINAL && panel != XDOS_PANEL_PLAYER && panel != XDOS_PANEL_MUSIC) ||
        !title || !hint || !buttons || !action || !request) return -1;
    const int width = LV_MIN(750, desktop_->Width()-24);
    const int height = LV_MIN(550, desktop_->Height()-64);
    if (width < 270 || height < 205) return -1;
    if (panel==XDOS_PANEL_FILES) return -1;
    if (panel==XDOS_PANEL_MUSIC) title="Arduino MP3 Player";
    auto *body = OpenAppWindow(title,width,height);
    if (!body) return -1;
    workspaceInput_ = MakeWindowTextArea(body);
    lv_textarea_set_one_line(workspaceInput_,true);
    lv_textarea_set_max_length(workspaceInput_,191);
    lv_textarea_set_placeholder_text(workspaceInput_,hint);
    if (panel == XDOS_PANEL_HTTP) lv_textarea_set_text(workspaceInput_,desktop_->BrowserURL());
    lv_obj_set_width(workspaceInput_,lv_pct(94));
    lv_obj_align(workspaceInput_,LV_ALIGN_TOP_MID,0,8);
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
        auto *button = MakeWindowButton(body,label,WorkspaceButton,this);
        workspaceButtons_[i]=button;
        const int column = LV_MIN(146,(width-24)/3);
        lv_obj_set_width(button,column-8);
        lv_obj_align(button,LV_ALIGN_TOP_LEFT,12+column*i,51);
    }
    workspaceStatus_ = lv_label_create(body);
    lv_obj_align(workspaceStatus_,LV_ALIGN_TOP_LEFT,12,93);
    workspaceOutput_ = MakeWindowTextArea(body);
    lv_obj_set_size(workspaceOutput_,lv_pct(94),height-166);
    lv_obj_align(workspaceOutput_,LV_ALIGN_TOP_MID,0,119);
    lv_textarea_set_cursor_click_pos(workspaceOutput_,false);
    workspaceAction_=action;
    workspaceRequest_=request;
    workspaceContext_=context;
    workspacePanel_=panel;
    workspaceRevision_=~0U;

    if (panel == XDOS_PANEL_PLAYER || panel == XDOS_PANEL_MUSIC) {
        lv_obj_add_flag(workspaceOutput_,LV_OBJ_FLAG_HIDDEN);
        auto *picker=lv_dropdown_create(body);
        lv_obj_set_width(picker,lv_pct(94));
        lv_obj_align(picker,LV_ALIGN_TOP_MID,0,119);
        lv_dropdown_set_options(picker,panel==XDOS_PANEL_MUSIC ? "Choose MP3 or WAV from SD" : "Choose an MP4 from SD");
        const char *folders[]={"SD:/","SD:/XDOS/INBOX/","SD:/XDOS/VIDEO/","SD:/XDOS/MUSIC/"};
        unsigned listed=0;
        for (const char *folder : folders) {
            DIR dir;
            if (f_opendir(&dir,folder)!=FR_OK) continue;
            FILINFO entry;
            while (listed<128 && f_readdir(&dir,&entry)==FR_OK && entry.fname[0]) {
                const size_t n=strlen(entry.fname);
                if ((entry.fattrib&AM_DIR) || n<5 || (panel==XDOS_PANEL_MUSIC ? (strcasecmp(entry.fname+n-4,".wav") && strcasecmp(entry.fname+n-4,".mp3"))
                                            : strcasecmp(entry.fname+n-4,".mp4")) ||
                    strchr(entry.fname,'\n') || strlen(folder)+n>=191) continue;
                char path[192];
                strcpy(path,folder);
                strcat(path,entry.fname);
                lv_dropdown_add_option(picker,path,LV_DROPDOWN_POS_LAST);
                ++listed;
            }
            f_closedir(&dir);
        }
        lv_obj_add_event_cb(picker,[](lv_event_t *event) {
            auto *input=static_cast<lv_obj_t *>(lv_event_get_user_data(event));
            auto *dropdown=lv_event_get_target_obj(event);
            if (!lv_dropdown_get_selected(dropdown)) return;
            char selection[192];
            lv_dropdown_get_selected_str(dropdown,selection,sizeof selection);
            lv_textarea_set_text(input,selection);
        },LV_EVENT_VALUE_CHANGED,workspaceInput_);
    }
    if (panel == XDOS_PANEL_FILES) WorkspaceCommand(0);
    return 0;
}

int32_t XDOSLVGL::RunArduinoSketch(XdosSketchRoutine setup, XdosSketchRoutine loop,
                                     XdosSketchRoutine stop, bool measurements)
{
    if (!AtDesktop() || demoScreen_ || !setup || !loop) return -1;
    demoScreen_ = lv_obj_create(nullptr);
    if (!demoScreen_) return -1;
    SuspendMonitor();
    sketchActive_ = true;
    sketchMeasurements_ = measurements;
    sketchLoop_ = loop;
    sketchStop_ = stop;
    lv_screen_load(demoScreen_);
    if (measurements) lv_sysmon_performance_resume(lv_display_get_default());
    setup();
    if (XDOS_TEST_DEBUG && serial_) {
        static const char message[] = "[XAPP] Arduino LVGL setup completed\r\n";
        serial_->Write(message,sizeof message-1);
    }
    ShowAppTask(measurements ? "LVGL Benchmark" : "LVGL Demo");
    demoBack_ = MakeButton(lv_layer_top(),"BACK TO DESKTOP",DemoBack,this);
    lv_obj_set_size(demoBack_,168,37);
    lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    return 0;
}

void XDOSLVGL::Refresh(unsigned)
{
    if (!demoScreen_ && appTask_) ClearAppTask();
    if (!desktopCanvas_ || lv_screen_active() != homeScreen_) return;
    if (AtDesktop()) {
        bool pointerFound=false;
        for (auto *input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input)) {
            if (lv_indev_get_type(input)!=LV_INDEV_TYPE_POINTER) continue;
            lv_point_t p;
            lv_indev_get_point(input,&p);
            if (XappV2Runtime::BlocksDesktopHover(p.x, p.y))
                desktop_->UpdateDesktopPointer(-1,-1);
            else
            desktop_->UpdateDesktopPointer(p.x,p.y);
            pointerFound=true;
            break;
        }
        if (!pointerFound) desktop_->UpdateDesktopPointer(-1,-1);
    }
    if (!desktop_->NeedsRedraw()) return;
    int x,y,w,h;
    desktop_->GetDamage(x,y,w,h);
    desktop_->RenderDamage(desktopPixels_,desktop_->Width()*2,16,x,y,w,h);
    desktop_->ClearDamage();
    if (x==0 && y==0 && w==desktop_->Width() && h==desktop_->Height()) {
        lv_obj_invalidate(desktopCanvas_);
        return;
    }
    lv_area_t canvas;
    lv_obj_get_coords(desktopCanvas_,&canvas);
    const lv_area_t dirty={canvas.x1+x,canvas.y1+y,canvas.x1+x+w-1,canvas.y1+y+h-1};
    lv_obj_invalidate_area(desktopCanvas_,&dirty);
}

void XDOSLVGL::ShowAppTask(const char *title)
{
    ClearAppTask();
    appTask_=MakeWindowButton(lv_layer_top(),title,AppTaskPressed,this);
    const int width=desktop_->Width();
    lv_obj_set_size(appTask_,width>=800 ? 210 : 150,33);
    lv_obj_align(appTask_,LV_ALIGN_BOTTOM_LEFT,146,-9);
}

void XDOSLVGL::ClearAppTask()
{
    if (appTask_) lv_obj_delete(appTask_);
    appTask_=nullptr;
    appTaskFocused_=appMaximized_=appMinimized_=false;
}

void XDOSLVGL::FocusAppTask(bool focused)
{
    if (!appTask_) return;
    appTaskFocused_=focused;
    lv_obj_set_style_bg_color(appTask_,lv_color_hex(focused ? 0x698AA2 : 0x294A6A),0);
}

void XDOSLVGL::ToggleAppMaximized()
{
    if (!demoScreen_ || lv_obj_get_parent(demoScreen_)!=homeScreen_ || windowClosing_) return;
    lv_obj_update_layout(demoScreen_);
    if (!appMaximized_) {
        appRestoreX_=lv_obj_get_x(demoScreen_);
        appRestoreY_=lv_obj_get_y(demoScreen_);
        appRestoreW_=lv_obj_get_width(demoScreen_);
        appRestoreH_=lv_obj_get_height(demoScreen_);
        const int w=desktop_->Width(), h=desktop_->Height()-49;
        lv_obj_set_pos(demoScreen_,0,0);
        lv_obj_set_size(demoScreen_,w,h);
        lv_obj_set_size(appWindowBody_,w-2,h-59);
    } else {
        lv_obj_set_pos(demoScreen_,appRestoreX_,appRestoreY_);
        lv_obj_set_size(demoScreen_,appRestoreW_,appRestoreH_);
        lv_obj_set_size(appWindowBody_,appRestoreW_-2,appRestoreH_-59);
    }
    appMaximized_=!appMaximized_;
}

void XDOSLVGL::RestoreAppTask()
{
    if (!demoScreen_ || windowClosing_) return;
    if (appMinimized_) {
        lv_obj_remove_flag(demoScreen_,LV_OBJ_FLAG_HIDDEN);
        appMinimized_=false;
        if (!appMaximized_) ToggleAppMaximized();
    }
    FocusAppTask(false);
}

void XDOSLVGL::AppTaskPressed(lv_event_t *event)
{
    auto *ui=static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    if (ui->appMinimized_) ui->RestoreAppTask();
    else ui->FocusAppTask(true);
}

void XDOSLVGL::AppMinimizePressed(lv_event_t *event)
{
    auto *ui=static_cast<XDOSLVGL *>(lv_event_get_user_data(event));
    if (ui->windowClosing_) return;
    lv_obj_add_flag(ui->demoScreen_,LV_OBJ_FLAG_HIDDEN);
    ui->appMinimized_=true;
    ui->FocusAppTask(false);
}

void XDOSLVGL::AppMaximizePressed(lv_event_t *event)
{
    static_cast<XDOSLVGL *>(lv_event_get_user_data(event))->ToggleAppMaximized();
}

bool XDOSLVGL::HandleTaskKey(unsigned key)
{
    if (!demoScreen_ || !appTask_) return false;
    if (key==XdosKeyAltF4 || (appTaskFocused_ && key==XdosKeyDelete)) {
        if (appMinimized_) {
            lv_obj_remove_flag(demoScreen_,LV_OBJ_FLAG_HIDDEN);
            appMinimized_=false;
        }
        CloseCurrentApp();
        return true;
    }
    if (key=='\t') {
        FocusAppTask(!appTaskFocused_);
        return true;
    }
    if (appTaskFocused_ && (key=='\r' || key=='\n')) {
        RestoreAppTask();
        return true;
    }
    return appMinimized_;
}

void XDOSLVGL::SetAppTaskTitle(const char *title)
{
    if (appTask_) lv_label_set_text(lv_obj_get_child(appTask_,0),title);
}

#include "xdos_directory.h"

extern "C" int xdos_directory_visit(const char *path, unsigned offset, unsigned capacity,
                                    int directoriesOnly, XdosDirectoryVisitor visitor,
                                    void *context, unsigned *emitted, int *more)
{
    if (!path || !visitor || !emitted || !more || !capacity) return FR_INVALID_PARAMETER;
    *emitted=0;
    *more=0;
    if (capacity>XDOS_DIRECTORY_LIMIT) capacity=XDOS_DIRECTORY_LIMIT;
    DIR dir;
    auto result=f_opendir(&dir,path);
    if (result!=FR_OK) return result;
    FILINFO entry;
    for (;;) {
        result=f_readdir(&dir,&entry);
        if (result!=FR_OK || !entry.fname[0]) break;
        const bool directory=(entry.fattrib&AM_DIR)!=0;
        if (directoriesOnly && !directory) continue;
        if (offset) {
            --offset;
            continue;
        }
        if (*emitted==capacity) {
            *more=1;
            break;
        }
        visitor(context,entry.fname,directory);
        ++*emitted;
    }
    const auto closed=f_closedir(&dir);
    return result==FR_OK ? closed : result;
}

namespace {
lv_obj_t *filesPanes[2];
bool filesClaimed;
char filesLaunch[512];
}

bool XdosFilesClaim()
{
    if (filesClaimed) return false;
    filesClaimed=true;
    return true;
}

void XdosFilesSetPanes(lv_obj_t *tree, lv_obj_t *list)
{
    filesPanes[0]=list;
    filesPanes[1]=tree;
    filesClaimed=tree!=nullptr;
}

int XdosFilesQueueLaunch(const char *path)
{
    if (filesLaunch[0] || strlen(path)>=sizeof filesLaunch) return -1;
    strcpy(filesLaunch,path);
    return 0;
}

const char *XdosFilesTakeLaunch()
{
    static char path[sizeof filesLaunch];
    if (!filesLaunch[0]) return nullptr;
    strcpy(path,filesLaunch);
    filesLaunch[0]=0;
    return path;
}

int XdosFilesPreview(const char *path, char *buffer, unsigned limit, unsigned *read, int *more)
{
    FIL file;
    auto result=f_open(&file,path,FA_READ);
    if (result!=FR_OK) return result;
    result=f_read(&file,buffer,limit,read);
    *more=f_size(&file)>*read;
    const auto closed=f_close(&file);
    return result==FR_OK ? closed : result;
}

extern "C" bool xdos_files_wheel(int x, int y, int delta)
{
    if (!filesPanes[0]) return false;
    lv_point_t point{x,y};
    auto *target=lv_indev_search_obj(lv_layer_top(),&point);
    for (; target; target=lv_obj_get_parent(target)) {
        if (target!=filesPanes[0] && target!=filesPanes[1]) continue;
        auto *pane=target;
        const int top = lv_obj_get_scroll_top(pane);
        const int bottom = lv_obj_get_scroll_bottom(pane);
        const int position = LV_CLAMP(0, top - delta, LV_MAX(0, top + bottom));
        lv_obj_scroll_to_y(pane, position, LV_ANIM_OFF);
        return true;
    }
    return false;
}
