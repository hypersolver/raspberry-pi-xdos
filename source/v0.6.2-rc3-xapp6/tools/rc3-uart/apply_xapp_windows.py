"""Use LVGL desktop windows for the built-in XAPP host interfaces."""

from pathlib import Path
import sys


source = Path(sys.argv[1])
driver = source / "xdos_lvgl.cpp"
header = source / "xdos_lvgl.h"


def replace(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"Expected one XDOS window anchor: {old!r}")
    return text.replace(old, new)


def replace_range(text, start, stop, replacement):
    begin = text.index(start)
    end = text.index(stop, begin)
    return text[:begin] + replacement + text[end:]


def edit_app(text, name, next_function, transform):
    start = text.index(f"int32_t XDOSLVGL::{name}(")
    end = text.index(next_function, start)
    return text[:start] + transform(text[start:end]) + text[end:]


def keypad(app):
    app = replace_range(app, "    demoScreen_ = lv_obj_create", "    keypadDisplay_ =", """    const int width = LV_MIN(362, desktop_->Width()-20);
    const int height = LV_MIN(365, desktop_->Height()-24);
    if (width < 130 || height < 140) return -1;
    auto *panel = OpenAppWindow(title,width,height);
    if (!panel) return -1;
    const int column = (width-22)/4;
    const int row = (height-97)/4;
""")
    app = replace(app, "-8,30", "-10,12")
    app = replace(app, "MakeButton(panel", "MakeWindowButton(panel")
    app = replace(app, "74,48", "column-5,row-5")
    app = replace(app, "8+(i%4)*81,62+(i/4)*54", "10+(i%4)*column,49+(i/4)*row")
    app = replace_range(app, "    demoBack_ =", "    for (auto &byte", "")
    return replace(app, "    lv_screen_load(demoScreen_);\n", "")


def editor(app):
    app = replace_range(app, "    demoScreen_ = lv_obj_create", "    auto *save", """    const int width = LV_MIN(710, desktop_->Width()-24);
    const int height = LV_MIN(525, desktop_->Height()-64);
    if (width < 200 || height < 180) return -1;
    auto *panel = OpenAppWindow(title,width,height);
    if (!panel) return -1;
""")
    app = app.replace("MakeButton(demoScreen_", "MakeWindowButton(panel")
    app = replace(app, "lv_textarea_create(demoScreen_)", "MakeWindowTextArea(panel)")
    app = replace(app, "lv_pct(94),lv_pct(75)", "width-26,height-125")
    app = replace(app, ",12,42", ",12,8")
    app = replace(app, ",102,42", ",102,8")
    app = replace(app, ",0,90", ",0,53")
    app = replace(app, "lv_label_create(demoScreen_)", "lv_label_create(panel)")
    app = replace_range(app, "    demoBack_ =", "    editorAction_ =", "")
    return replace(app, "    lv_screen_load(demoScreen_);\n", "")


def paint(app):
    app = replace(app, "    const int width = desktop_->Width() > 640 ? 608 : desktop_->Width()-32;",
                  "    const int width = LV_MIN(608, desktop_->Width()-46) & ~1;")
    app = replace(app, "    const int height = desktop_->Height() > 530 ? 440 : desktop_->Height()-100;",
                  "    const int height = LV_MIN(440, desktop_->Height()-146);")
    app = replace_range(app, "    demoScreen_ = lv_obj_create", "    auto *clear", """    auto *panel = OpenAppWindow(title,width+22,height+82);
    if (!panel) {
        if (fresh) { delete[] surface_.pixels; surface_ = {}; }
        return -1;
    }
""")
    app = replace(app, 'MakeButton(demoScreen_,"CLEAR"', 'MakeWindowButton(panel,"CLEAR"')
    app = replace(app, ",92,5", ",10,4")
    app = replace(app, "lv_canvas_create(demoScreen_)", "lv_canvas_create(panel)")
    app = replace(app, ",0,62", ",0,42")
    app = replace_range(app, "    demoBack_ =", "    memset(strokeState_", "")
    return replace(app, "    lv_screen_load(demoScreen_);\n", "")


def workspace(app):
    app = replace_range(app, "    demoScreen_ = lv_obj_create", "    lv_textarea_set_one_line", """    const bool windowed = panel != XDOS_PANEL_UPDATE;
    const int width = LV_MIN(750, desktop_->Width()-24);
    const int height = LV_MIN(550, desktop_->Height()-64);
    if (windowed && (width < 270 || height < 205)) return -1;
    lv_obj_t *body;
    if (windowed) {
        body = OpenAppWindow(title,width,height);
        if (!body) return -1;
    } else {
        demoScreen_ = lv_obj_create(nullptr);
        if (!demoScreen_) return -1;
        body = demoScreen_;
        SuspendMonitor();
        auto *heading = lv_label_create(body);
        lv_label_set_text(heading,title);
        lv_obj_align(heading,LV_ALIGN_TOP_LEFT,12,12);
    }
    workspaceInput_ = windowed ? MakeWindowTextArea(body) : lv_textarea_create(body);
""")
    app = replace(app, ",0,48", ",0,windowed ? 8 : 48")
    app = replace(app, "auto *button = MakeButton(demoScreen_,label,WorkspaceButton,this);",
                  """auto *button = windowed ? MakeWindowButton(body,label,WorkspaceButton,this) :
                                    MakeButton(body,label,WorkspaceButton,this);""")
    app = replace(app, "lv_obj_set_width(button,138);",
                  "const int column = windowed ? LV_MIN(146,(width-24)/3) : 146;\n"
                  "        lv_obj_set_width(button,column-8);")
    app = replace(app, "12+146*i,94", "12+column*i,windowed ? 51 : 94")
    app = replace(app, "lv_label_create(demoScreen_)", "lv_label_create(body)")
    app = replace(app, ",12,142", ",12,windowed ? 93 : 142")
    app = replace(app, "lv_textarea_create(demoScreen_)",
                  "windowed ? MakeWindowTextArea(body) : lv_textarea_create(body)")
    app = replace(app, "lv_pct(94),lv_pct(68)", "lv_pct(94),windowed ? height-166 : lv_pct(68)")
    app = replace(app, ",0,168", ",0,windowed ? 119 : 168")
    app = replace_range(app, "    demoBack_ =", "    workspaceAction_=", """    if (!windowed) {
        demoBack_ = MakeButton(demoScreen_,"BACK TO DESKTOP",DemoBack,this);
        lv_obj_set_size(demoBack_,168,37);
        lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    }
""")
    return replace(app, "    lv_screen_load(demoScreen_);", "    if (!windowed) lv_screen_load(demoScreen_);")


cpp = driver.read_text(encoding="utf-8")
cpp = replace(cpp, '#include "xdos_lvgl.h"',
              '#include "xdos_lvgl.h"\n#include <lvgl/src/misc/lv_timer_private.h>')
cpp = replace(cpp, "void XDOSLVGL::DesktopLoaded",
              Path(__file__).with_name("xapp_window.inc").read_text(encoding="utf-8") +
              "\nvoid XDOSLVGL::DesktopLoaded")
cpp = replace(cpp, "    return homeScreen_ && lv_screen_active() == homeScreen_;",
              "    return !demoScreen_ && homeScreen_ && lv_screen_active() == homeScreen_;")
cpp = replace(cpp, "    if (!input) return;\n    lv_point_t pt;",
              "    if (!input || !ui->AtDesktop()) return;\n    lv_point_t pt;")
cpp = replace(cpp, """    ui->keypadDisplay_ = nullptr;
    lv_obj_delete_async(ui->demoBack_);""", """    ui->keypadDisplay_ = nullptr;
    if (lv_obj_get_parent(ui->demoScreen_) == ui->homeScreen_) {
        ui->BeginAppWindowClose();
        return;
    }
    lv_obj_delete_async(ui->demoBack_);""")
cpp = edit_app(cpp, "OpenKeypad", "\nvoid XDOSLVGL::EditorButton", keypad)
cpp = edit_app(cpp, "OpenEditor", "\nvoid XDOSLVGL::PaintEvent", editor)
cpp = edit_app(cpp, "OpenCanvas", "\nvoid XDOSLVGL::WorkspaceButton", paint)
cpp = edit_app(cpp, "OpenWorkspace", "\nint32_t XDOSLVGL::RunArduinoSketch", workspace)
declarations = replace(header.read_text(encoding="utf-8"), "    void SuspendMonitor();",
                       "    lv_obj_t *OpenAppWindow(const char *title, int width, int height);\n"
                       "    static void AppWindowClosed(lv_anim_t *animation);\n"
                       "    static void AppWindowOpened(lv_anim_t *animation);\n"
                       "    void BoostWindowTimers();\n"
                       "    void RestoreWindowTimers();\n"
                       "    void BeginAppWindowClose();\n"
                       "    void FinishAppWindowClose();\n"
                       "    bool windowClosing_ = false;\n"
                       "    bool windowTimersBoosted_ = false;\n"
                       "    uint32_t windowRefreshPeriod_ = 0, windowAnimationPeriod_ = 0;\n"
                       "    void SuspendMonitor();")
declarations = replace(declarations, "    bool AtDesktop() const;",
                       "    bool AtDesktop() const;\n    bool CloseCurrentApp();")
driver.write_text(cpp, encoding="utf-8")
header.write_text(declarations, encoding="utf-8")
