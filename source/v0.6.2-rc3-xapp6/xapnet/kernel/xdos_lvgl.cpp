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
constexpr int WindowScaleNormal = 256;
constexpr int WindowScaleSmall = 230;

void SetWindowScale(void *object, int32_t scale)
{
    auto *window = static_cast<lv_obj_t *>(object);
    lv_obj_set_style_transform_scale_x(window, scale, 0);
    lv_obj_set_style_transform_scale_y(window, scale, 0);
}

void SetClosingWindowScale(void *object, int32_t scale)
{
    SetWindowScale(object, scale);
    lv_obj_set_style_opa_layered(static_cast<lv_obj_t *>(object),
        (scale - WindowScaleSmall) * LV_OPA_COVER / (WindowScaleNormal - WindowScaleSmall), 0);
}

lv_anim_t *StartWindowAnimation(lv_obj_t *window, bool closing, void *owner,
                                lv_anim_completed_cb_t completed)
{
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, window);
    lv_anim_set_user_data(&animation, owner);
    lv_anim_set_values(&animation,
        closing ? lv_obj_get_style_transform_scale_x(window, LV_PART_MAIN) : WindowScaleSmall,
        closing ? WindowScaleSmall : WindowScaleNormal);
    lv_anim_set_duration(&animation, closing ? 140 : 160);
    lv_anim_set_exec_cb(&animation, closing ? SetClosingWindowScale : SetWindowScale);
    lv_anim_set_path_cb(&animation, closing ? lv_anim_path_ease_in : lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&animation, completed);
    return lv_anim_start(&animation);
}

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
    lv_obj_set_style_transform_pivot_x(window, width/2, 0);
    lv_obj_set_style_transform_pivot_y(window, height/2, 0);

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
    if (StartWindowAnimation(window, false, this, AppWindowOpened))
        BoostWindowTimers();
    else
        SetWindowScale(window, WindowScaleNormal);
    overwriteText_ = false;
    return body;
}

void XDOSLVGL::BoostWindowTimers()
{
    if (windowTimersBoosted_) return;
    auto *refresh = lv_display_get_refr_timer(lv_obj_get_display(demoScreen_));
    auto *animation = lv_anim_get_timer();
    if (refresh) {
        windowRefreshPeriod_ = refresh->period;
        lv_timer_set_period(refresh, 16);
    }
    if (animation) {
        windowAnimationPeriod_ = animation->period;
        lv_timer_set_period(animation, 16);
    }
    windowTimersBoosted_ = true;
}

void XDOSLVGL::RestoreWindowTimers()
{
    if (!windowTimersBoosted_) return;
    auto *refresh = lv_display_get_refr_timer(lv_obj_get_display(demoScreen_));
    auto *animation = lv_anim_get_timer();
    if (refresh) lv_timer_set_period(refresh, windowRefreshPeriod_);
    if (animation) lv_timer_set_period(animation, windowAnimationPeriod_);
    windowTimersBoosted_ = false;
}

void XDOSLVGL::AppWindowOpened(lv_anim_t *animation)
{
    static_cast<XDOSLVGL *>(lv_anim_get_user_data(animation))->RestoreWindowTimers();
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
    lv_anim_delete(window, SetWindowScale);
    if (StartWindowAnimation(window, true, this, AppWindowClosed))
        BoostWindowTimers();
    else
        FinishAppWindowClose();
}

void XDOSLVGL::AppWindowClosed(lv_anim_t *animation)
{
    static_cast<XDOSLVGL *>(lv_anim_get_user_data(animation))->FinishAppWindowClose();
}

void XDOSLVGL::FinishAppWindowClose()
{
    RestoreWindowTimers();
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
    if (panel==XDOS_PANEL_FILES) title="Arduino Files";
    if (panel==XDOS_PANEL_MUSIC) title="Arduino MP3 Player";
    auto *body = OpenAppWindow(title,width,height);
    if (!body) return -1;
    if (panel==XDOS_PANEL_FILES) {
        OpenFileTree(body);
        return 0;
    }
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
    char deleteTarget[512];
    char launchTarget[512];
    uint32_t launchClickTick;
    lv_obj_t *selectedRow;
};
FileBrowser *activeFileBrowser;

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

extern "C" bool xdos_files_wheel(int x, int y, int delta)
{
    if (!activeFileBrowser) return false;
    lv_obj_t *const panes[] = {activeFileBrowser->list, activeFileBrowser->tree};
    for (auto *pane : panes) {
        if (!pane || lv_obj_get_screen(pane) != lv_screen_active()) continue;
        lv_area_t bounds;
        lv_obj_get_coords(pane, &bounds);
        if (x < bounds.x1 || x > bounds.x2 || y < bounds.y1 || y > bounds.y2) continue;
        const int top = lv_obj_get_scroll_top(pane);
        const int bottom = lv_obj_get_scroll_bottom(pane);
        const int position = LV_CLAMP(0, top - delta, LV_MAX(0, top + bottom));
        lv_obj_scroll_to_y(pane, position, LV_ANIM_OFF);
        return true;
    }
    return false;
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
    lv_obj_set_style_bg_color(button,lv_color_hex(0xFFFFFF),LV_STATE_CHECKED);
    lv_obj_set_style_text_color(button,lv_color_hex(0x203047),LV_STATE_CHECKED);
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
        delete static_cast<FileTreeNode *>(lv_event_get_user_data(event));
    },LV_EVENT_DELETE,node);
}

void XDOSLVGL::FileTreeClick(lv_event_t *event)
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
            if (node.ui->windowClosing_) return;
            strcpy(node.ui->fileTreeLaunch_,node.path);
            node.ui->fileTreeLaunchPending_=true;
            node.ui->CloseCurrentApp();
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
    browser.deleteTarget[0] = 0;
    browser.launchTarget[0] = 0;
    if (browser.selectedRow) lv_obj_remove_state(browser.selectedRow, LV_STATE_CHECKED);
    browser.selectedRow = nullptr;
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
    activeFileBrowser = browser;
    lv_obj_add_event_cb(body,[](lv_event_t *event) {
        auto *browser = static_cast<FileBrowser *>(lv_event_get_user_data(event));
        if (activeFileBrowser == browser) activeFileBrowser = nullptr;
        delete browser;
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
    const int buttonWidth=LV_MIN(106,(desktop_->Width()-45)/7);
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
        const auto result = XdosDeleteFile(b->deleteTarget);
        b->deleteTarget[0] = 0;
        if (result == FR_OK) b->ui->RenderFiles(b);
        lv_textarea_set_text(b->preview,result == FR_OK ? "File deleted."
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
