"""Connect the external Calculator to a host-owned keypad session."""

from pathlib import Path
import re
import shutil
import sys


source = Path(sys.argv[1])


def replace(file, old, new):
    path = source / file
    content = path.read_text(encoding="utf-8")
    if content.count(old) != 1:
        raise RuntimeError(f"Unexpected source layout in {path}: {old!r}")
    path.write_text(content.replace(old, new), encoding="utf-8")


def remove_function(file, name):
    path = source / file
    content = path.read_text(encoding="utf-8")
    pattern = rf"(?m)^void Desktop::{name}\([^\n]*\)\n\{{\n.*?^\}}\n"
    content, count = re.subn(pattern, "", content, flags=re.S)
    if count != 1:
        raise RuntimeError(f"Missing {name}: {path}")
    path.write_text(content, encoding="utf-8")


shutil.copyfile(Path(__file__).with_name("xapp_keypad.h"), source / "xapp_keypad.h")
replace("desktop.h", "    bool TakeAppLaunchRequest();", "    bool TakeCalculatorRequest();\n    bool TakeAppLaunchRequest();")
replace("desktop.h", "    char terminal_[2048], note_[2048], cmd_[160], calcDisplay_[32];",
        "    char terminal_[2048], note_[2048], cmd_[160];")
replace("desktop.h", """    unsigned termLen_, noteLen_, cmdLen_, calcLen_;
    long calcValue_, calcEntry_;
    char calcOp_;
    bool calcNewEntry_;""", """    unsigned termLen_, noteLen_, cmdLen_;
    bool calculatorPending_ = false;""")
replace("desktop.h", "    void TapCalculator(int x,int y);\n", "")
replace("desktop.h", "    void DrawCalculator(int i);\n", "")
replace("desktop.cpp", """    termLen_(0),noteLen_(0),cmdLen_(0),calcLen_(0),
    calcValue_(0),calcEntry_(0),calcOp_(0),calcNewEntry_(false),""",
        "    termLen_(0),noteLen_(0),cmdLen_(0),")
replace("desktop.cpp", '    strcpy(calcDisplay_,"0");\n', "")
replace("desktop.cpp", "void Desktop::Open(int i)\n{", """bool Desktop::TakeCalculatorRequest()
{
    const bool pending = calculatorPending_;
    calculatorPending_ = false;
    return pending;
}

void Desktop::Open(int i)
{
    if (i == Calculator) {
        calculatorPending_ = true;
        return;
    }""")
replace("desktop.cpp", "        if(i==Calculator) TapCalculator(x,y);\n", "")
replace("desktop.cpp", "    case Calculator:DrawCalculator(i);break;case Paint:DrawPaint(i);break;",
        "    case Paint:DrawPaint(i);break;")
remove_function("desktop.cpp", "TapCalculator")
remove_function("desktop.cpp", "DrawCalculator")

replace("app_shortcut.cpp", "const char *XdosRunLVGLShortcut(", """const char *XdosRunCalculatorShortcut(const XdosHostApiV1 *api)
{
    static const char manifest[] = "SD:/XDOS/APPS/CALC.APP";
    static const char module[] = "SD:/XDOS/APPS/CALC.XAPP";
    if (!VerifyManifest(manifest, module)) return "XAPP: CALC.APP missing or invalid";
    const auto result = XappLoaderV1::ExecuteFile(module, api);
    if (result == XappLoaderV1::OK) return "XAPP: CALC.XAPP started";
    if (result == XappLoaderV1::OPEN_FAILED) return "XAPP: CALC.XAPP missing";
    return "XAPP: Calculator load or launch failed";
}

const char *XdosRunLVGLShortcut(""")
replace("app_shortcut.h", "const char *XdosRunLVGLShortcut(", """const char *XdosRunCalculatorShortcut(const XdosHostApiV1 *api);
const char *XdosRunLVGLShortcut(""")

replace("xdos_lvgl.h", '#include "xdos_benchmark.h"',
        '#include "xdos_benchmark.h"\n#include "xapp_keypad.h"')
replace("xdos_lvgl.h", "    void OnKey(char ch);", """    bool OnKey(char ch);
    bool AtDesktop() const;
    int32_t OpenKeypad(const char *title, const char *keys, XdosKeypadInput input);""")
replace("xdos_lvgl.h", "    static void DemoBack(lv_event_t *event);", """    static void DemoBack(lv_event_t *event);
    static void KeypadButton(lv_event_t *event);
    void KeypadKey(int key);
    XdosKeypadInput keypadInput_ = nullptr;
    lv_obj_t *keypadDisplay_ = nullptr;
    alignas(uint32_t) unsigned char keypadState_[32] = {};
    char keypadText_[32] = {};""")
replace("xdos_lvgl.cpp", "    lv_obj_delete_async(ui->demoBack_);", """    ui->keypadInput_ = nullptr;
    ui->keypadDisplay_ = nullptr;
    lv_obj_delete_async(ui->demoBack_);""")
replace("xdos_lvgl.cpp", "void XDOSLVGL::OnKey(char ch) {(void)ch;}", """bool XDOSLVGL::AtDesktop() const
{
    return homeScreen_ && lv_screen_active() == homeScreen_;
}

bool XDOSLVGL::OnKey(char ch)
{
    if (!keypadInput_) return !AtDesktop();
    KeypadKey(static_cast<unsigned char>(ch));
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
    demoScreen_ = lv_obj_create(nullptr);
    if (!demoScreen_) return -1;
    SuspendMonitor();
    auto *panel = lv_obj_create(demoScreen_);
    lv_obj_set_size(panel,360,346);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    auto *heading = lv_label_create(panel);
    lv_label_set_text(heading,title);
    lv_obj_set_pos(heading,8,0);
    keypadDisplay_ = lv_label_create(panel);
    lv_obj_align(keypadDisplay_,LV_ALIGN_TOP_RIGHT,-8,30);
    for (unsigned i = 0; i < 16; ++i) {
        const char label[] = {keys[i],0};
        auto *key = MakeButton(panel,label,KeypadButton,this);
        lv_obj_set_size(key,74,48);
        lv_obj_set_pos(key,8+(i%4)*81,62+(i/4)*54);
    }
    demoBack_ = MakeButton(demoScreen_,"BACK TO DESKTOP",DemoBack,this);
    lv_obj_set_size(demoBack_,168,37);
    lv_obj_align(demoBack_,LV_ALIGN_TOP_RIGHT,-8,5);
    for (auto &byte : keypadState_) byte = 0;
    keypadText_[0] = 0;
    keypadInput_ = input;
    KeypadKey(0);
    lv_screen_load(demoScreen_);
    return 0;
}""")
replace("kernel.cpp", '#include "uart_service.h"', '#include "uart_service.h"\n#include "xapp_keypad.h"')
replace("kernel.cpp", """        desktop_.Key(ch);
#if XDOS_USE_LVGL
        ui_.OnKey(ch);
#endif""", """#if XDOS_USE_LVGL
        if (ui_.OnKey(ch)) continue;
#endif
        desktop_.Key(ch);""")
replace("kernel.cpp", "        const bool openWidgets = desktop_.TakeAppLaunchRequest();", """#if XDOS_USE_LVGL
        if (desktop_.TakeCalculatorRequest() && ui_.AtDesktop()) {
            XdosKeypadApi keypad = {api, [](void *ctx, const char *title, const char *keys,
                                         XdosKeypadInput input) -> int32_t {
                return static_cast<CKernel *>(ctx)->ui_.OpenKeypad(title, keys, input);
            }};
            keypad.base.struct_bytes = sizeof keypad;
            desktop_.ReportAppResult(storageMounted_ ?
                XdosRunCalculatorShortcut(&keypad.base) : "XAPP: FAT32 NOT READY");
        }
#endif
        const bool openWidgets = desktop_.TakeAppLaunchRequest();""")
