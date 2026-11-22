"""Add the desktop XDOS button's Restart and Shut Down menu."""
from pathlib import Path
import sys

root = Path(sys.argv[1])

def patch(name, *changes):
    path = root / name
    text = path.read_text(encoding="utf-8")
    for old, new in changes:
        if text.count(old) != 1:
            raise SystemExit(f"{name}: missing or non-unique landmark: {old}")
        text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")

patch("desktop.h",
    ("    void ScanDesktopApps();", """    void ScanDesktopApps();
    enum class PowerRequest { None, Restart, ShutDown };
    PowerRequest TakePowerRequest();
    void ShowShutdownNotice() { shutdownNotice_=true; RequestFullRepaint(); }
    void DrawPowerText(int x,int y,const char *text,int scale);
    bool StorageWorkPending() const { return storagePending_ || storageBusy_; }"""),
    ("    unsigned discoveredCount_ = 0;", """    unsigned discoveredCount_ = 0;
    bool powerMenuOpen_ = false;
    bool shutdownNotice_ = false;
    bool shutdownBlank_ = false;
    PowerRequest powerRequest_ = PowerRequest::None;"""))

patch("desktop.cpp",
    ("void Desktop::Mouse(MouseAction action,unsigned buttons,int x,int y)", """namespace {
constexpr int PowerMenuX = 14;

struct PowerMenuLayout {
    int top, width, row, scale;
};

PowerMenuLayout GetPowerMenuLayout(int width,int height)
{
    const int scale=width>=1440 && height>=900 ? 3 : width>=640 && height>=400 ? 2 : 1;
    const int row=22*scale;
    return {height-49-4*scale-2*row,112*scale,row,scale};
}
}

void Desktop::DrawPowerText(int x,int y,const char *text,int scale)
{
    for (; *text; ++text,x+=8*scale) {
        for (int row=0;row<16;++row) {
            const auto bits=font.GetPixelLine(*text,row);
            for (int column=0;column<8;++column) {
                if (font.GetPixel(column,bits))
                    Fill(x+column*scale,y+row*scale,scale,scale,WHITE);
            }
        }
    }
}

Desktop::PowerRequest Desktop::TakePowerRequest()
{
    const auto request=powerRequest_;
    powerRequest_=PowerRequest::None;
    return request;
}

void Desktop::Mouse(MouseAction action,unsigned buttons,int x,int y)"""),
    ("    if(action!=Down||!(buttons&1)) return;", """    if(action!=Down||!(buttons&1)) return;
    if (Contains(x,y,14,height_-42,123,33)) {
        powerMenuOpen_=!powerMenuOpen_;
        RequestFullRepaint();
        return;
    }
    if (powerMenuOpen_) {
        const auto menu=GetPowerMenuLayout(width_,height_);
        if (Contains(x,y,PowerMenuX,menu.top,menu.width,2*menu.row))
            powerRequest_=y<menu.top+menu.row ? PowerRequest::Restart : PowerRequest::ShutDown;
        powerMenuOpen_=false;
        RequestFullRepaint();
        return;
    }"""),
    ("void Desktop::Key(char ch)\n{", """void Desktop::Key(char ch)
{
    if (powerMenuOpen_) {
        if (ch=='r' || ch=='R') powerRequest_=PowerRequest::Restart;
        else if (ch=='s' || ch=='S') powerRequest_=PowerRequest::ShutDown;
        else if (ch!=27) return;
        powerMenuOpen_=false;
        RequestFullRepaint();
        return;
    }"""),
    ('    Text(28,height_-33,"XD OS",NAV);', """    Text(28,height_-33,"XDOS",NAV);
    if (powerMenuOpen_) {
        const auto menu=GetPowerMenuLayout(width_,height_);
        const int labelX=PowerMenuX+8*menu.scale;
        const int labelY=menu.top+4*menu.scale;
        Fill(PowerMenuX,menu.top,menu.width,2*menu.row,BG2);
        Frame(PowerMenuX,menu.top,menu.width,2*menu.row,ACCENT);
        Fill(PowerMenuX+1,menu.top+menu.row,menu.width-2,1,0x42637A);
        DrawPowerText(labelX,labelY,"Restart",menu.scale);
        Fill(labelX,labelY+16*menu.scale,8*menu.scale,menu.scale,WHITE);
        DrawPowerText(labelX,labelY+menu.row,"Shutdown",menu.scale);
        Fill(labelX,labelY+menu.row+16*menu.scale,8*menu.scale,menu.scale,WHITE);
    }
    if (shutdownNotice_) {
        const int scale=GetPowerMenuLayout(width_,height_).scale;
        const int w=224*scale, h=70*scale;
        const int x=(width_-w)/2, y=(height_-h)/2;
        Fill(x+4,y+5,w,h,0x071522);
        Fill(x,y,w,h,BG2);
        Frame(x,y,w,h,ACCENT);
        Fill(x+1,y+28*scale,w-2,1,ACCENT);
        DrawPowerText(x+8*scale,y+6*scale,"XDOS Shut Down",scale);
        DrawPowerText(x+8*scale,y+42*scale,"Safe to disconnect power.",scale);
    }"""))

patch("uart_service.h", ("    bool TransferActive() const { return active_; }",
    """    bool TransferActive() const { return active_; }
    bool PowerTransferActive() const { return active_ || fetchState_==FetchState::Working; }"""))

patch("kernel.cpp",
    ('#include "lvgl_vc4.h"', '#include "lvgl_vc4.h"\n#include <circle/bcmpropertytags.h>'),
    ("        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();",
    """        const bool canOpen=ui_.AtDesktop() && !uart.TransferActive();
        const auto power=desktop_.TakePowerRequest();
        if (power!=Desktop::PowerRequest::None && ui_.AtDesktop()) {
            if (uart.PowerTransferActive() || desktop_.StorageWorkPending()) {
                desktop_.ReportAppResult("Power action unavailable while storage is busy");
            } else {
                const bool restart=power==Desktop::PowerRequest::Restart;
                if (storageMounted_) {
                    f_mount(nullptr,"SD:",0);
                    storageMounted_=false;
                }
                if (restart) desktop_.ReportAppResult("Restarting XDOS...");
                else desktop_.ShowShutdownNotice();
                ui_.Refresh(CTimer::GetClockTicks());
                lv_refr_now(lv_display_get_default());
                scheduler_.MsSleep(40);
                xdos_present_frame();
                Debug(restart ? "[POWER] Desktop restart\\r\\n" : "[POWER] Desktop shutdown\\r\\n");
                if (restart) reboot();
                scheduler_.MsSleep(2000);
                // The cursor lives on the system layer, above the desktop canvas.
                auto *black=lv_obj_create(lv_layer_sys());
                lv_obj_remove_style_all(black);
                lv_obj_set_size(black,lv_pct(100),lv_pct(100));
                lv_obj_set_pos(black,0,0);
                lv_obj_set_style_bg_color(black,lv_color_black(),0);
                lv_obj_set_style_bg_opa(black,LV_OPA_COVER,0);
                lv_obj_move_foreground(black);
                lv_refr_now(lv_display_get_default());
                xdos_present_frame();
                bool hdmiOff=xdos_vc4_power_off_hdmi();
                if (!hdmiOff) {
                    // Firmware framebuffer blank request (tag 0x00040002).
                    struct { TPropertyTag Tag; u32 state; } blank = {{},1};
                    hdmiOff=CBcmPropertyTags().GetTag(0x00040002,&blank,sizeof blank,sizeof blank.state);
                }
                Debug(hdmiOff ? "[POWER] Display off requested\\r\\n" :
                                "[POWER] Display off failed\\r\\n");
                scheduler_.MsSleep(150);
                halt();
            }
        }"""))

patch("lvgl_vc4.h", ("void xdos_vc4_stop(void);",
    "void xdos_vc4_stop(void);\nint xdos_vc4_power_off_hdmi(void);"))

with (root / "lvgl_vc4.c").open("a", encoding="utf-8") as out:
    out.write(r'''
/* Reuse the host connection opened by the VC4 presenter. */
int xdos_vc4_power_off_hdmi(void)
{
    VCHI_INSTANCE_T instance = 0;
    VCHI_CONNECTION_T *connection = 0;
    vc_host_get_vchi_state(&instance, &connection);
    if (!instance || !connection || vc_vchi_tv_init(instance, &connection, 1) != 0)
        return 0;
    return vc_tv_power_off() == 0;
}
''')

patch("desktop.h",
    ("    void DrawPowerText(", """    void UpdateDesktopPointer(int x,int y);
    bool DesktopNavigationKey(unsigned key);
    void DrawPowerText("""),
    ("    bool shutdownBlank_ = false;", """    int pointerX_ = -1, pointerY_ = -1;
    int iconFocus_ = -1, powerFocus_ = -1;
    void SetIconFocus(int index);
    int IconHighlightSize() const;
    void BlendWhite(int x,int y,int w,int h);"""))

patch("desktop.cpp",
    ('#include "desktop.h"', '#include "desktop.h"\n#include "xdos_text_keys.h"\n#include <stdint.h>'),
    ("Desktop::PowerRequest Desktop::TakePowerRequest()", """int Desktop::IconHighlightSize() const
{
    const auto slot=PlaceDiscoveredApp(width_,height_,0);
    const int glyphScale=width_>=1280 && height_>=900 ? 2 : 1;
    int side=slot.height;
    for (unsigned i=0;i<discoveredCount_;++i) {
        const unsigned count=strlen(discoveredApps_[i].name);
        if (count>16) continue;
        int labelWidth=int(count)*8*glyphScale;
        if (labelWidth>slot.width) labelWidth=int(count)*8;
        if (labelWidth<=slot.width && labelWidth>side) side=labelWidth;
    }
    return side<=slot.width ? side : slot.width;
}

void Desktop::SetIconFocus(int index)
{
    if (index==iconFocus_) return;
    const int side=IconHighlightSize();
    const int affected[]={iconFocus_,index};
    for (const int i : affected) {
        if (i<0 || unsigned(i)>=discoveredCount_) continue;
        const auto slot=PlaceDiscoveredApp(width_,height_,i);
        const int x=slot.x+(slot.width-side)/2;
        const int y=slot.y+(slot.height-side)/2;
        MarkDamage(x,y,x+side,y+side);
    }
    iconFocus_=index;
}

void Desktop::UpdateDesktopPointer(int x,int y)
{
    if (x==pointerX_ && y==pointerY_) return;
    pointerX_=x; pointerY_=y;
    if (powerMenuOpen_) {
        const auto menu=GetPowerMenuLayout(width_,height_);
        const int row=Contains(x,y,PowerMenuX,menu.top,menu.width,2*menu.row)
            ? (y-menu.top)/menu.row : -1;
        if (row!=powerFocus_) {
            powerFocus_=row;
            MarkDamage(PowerMenuX,menu.top,PowerMenuX+menu.width,menu.top+2*menu.row);
        }
        return;
    }
    int hovered=-1;
    const int side=IconHighlightSize();
    for (unsigned i=0;i<discoveredCount_;++i) {
        const auto slot=PlaceDiscoveredApp(width_,height_,i);
        if (Contains(x,y,slot.x+(slot.width-side)/2,slot.y+(slot.height-side)/2,side,side)) {
            hovered=int(i);
            break;
        }
    }
    SetIconFocus(hovered);
}

bool Desktop::DesktopNavigationKey(unsigned key)
{
    if (key==XdosKeyWindows) {
        powerMenuOpen_=!powerMenuOpen_;
        powerFocus_=-1;
        RequestFullRepaint();
        return true;
    }
    const bool enter=key=='\\r' || key=='\\n';
    if (powerMenuOpen_) {
        if (key==XdosKeyUp || key==XdosKeyDown) {
            powerFocus_=powerFocus_<0 ? (key==XdosKeyUp ? 1 : 0) : 1-powerFocus_;
            RequestFullRepaint();
            return true;
        }
        if (enter) {
            if (powerFocus_<0) return true;
            powerRequest_=powerFocus_==0 ? PowerRequest::Restart : PowerRequest::ShutDown;
            powerMenuOpen_=false;
            RequestFullRepaint();
            return true;
        }
        return false;
    }
    if (enter && iconFocus_>=0) {
        OpenDiscoveredApp(unsigned(iconFocus_));
        return true;
    }
    if (key!=XdosKeyLeft && key!=XdosKeyRight && key!=XdosKeyUp && key!=XdosKeyDown)
        return false;
    if (!discoveredCount_) return true;
    int index=iconFocus_;
    if (index<0) index=0;
    else if (key==XdosKeyLeft && index%5>0) --index;
    else if (key==XdosKeyRight && index%5<4 && unsigned(index+1)<discoveredCount_) ++index;
    else if (key==XdosKeyUp && index>=5) index-=5;
    else if (key==XdosKeyDown && index<5 && discoveredCount_>5)
        index=Clamp(index+5,5,int(discoveredCount_)-1);
    SetIconFocus(index);
    return true;
}

void Desktop::BlendWhite(int x,int y,int w,int h)
{
    const int x0=Clamp(x,clipRender_ ? clipX0_ : 0,width_);
    const int y0=Clamp(y,clipRender_ ? clipY0_ : 0,height_);
    const int x1=Clamp(x+w,0,clipRender_ ? clipX1_ : width_);
    const int y1=Clamp(y+h,0,clipRender_ ? clipY1_ : height_);
    // Blend 25% white with the wallpaper or menu surface already rendered.
    for (int row=y0;row<y1;++row) {
        auto *base=static_cast<unsigned char *>(framebuffer_)+row*pitchBytes_;
        if (depth_==16) {
            auto *pixels=reinterpret_cast<uint16_t *>(base);
            for (int col=x0;col<x1;++col) {
                const unsigned p=pixels[col], r=p>>11, g=(p>>5)&63, b=p&31;
                pixels[col]=uint16_t(((3*r+31)/4)<<11 | ((3*g+63)/4)<<5 | ((3*b+31)/4));
            }
        } else {
            auto *pixels=reinterpret_cast<uint32_t *>(base);
            for (int col=x0;col<x1;++col) {
                const unsigned p=pixels[col], r=(p>>16)&255, g=(p>>8)&255, b=p&255;
                pixels[col]=(p&0xFF000000u) | ((3*r+255)/4)<<16 |
                    ((3*g+255)/4)<<8 | ((3*b+255)/4);
            }
        }
    }
}

Desktop::PowerRequest Desktop::TakePowerRequest()"""),
    ("    discoveredPending_ = -1;\n    discoveredCount_", "    discoveredPending_ = -1;\n    iconFocus_ = -1;\n    discoveredCount_"),
    ("        powerMenuOpen_=!powerMenuOpen_;\n        RequestFullRepaint();", "        powerMenuOpen_=!powerMenuOpen_;\n        powerFocus_=-1;\n        RequestFullRepaint();"),
    ("        const int iconX=centerX-slot.icon/2;",
     """        const int iconX=centerX-slot.icon/2;
        if (int(i)==iconFocus_ && !powerMenuOpen_) {
            const int side=IconHighlightSize();
            BlendWhite(slot.x+(slot.width-side)/2,slot.y+(slot.height-side)/2,side,side);
        }"""),
    ("        Frame(PowerMenuX,menu.top,menu.width,2*menu.row,ACCENT);",
     """        if (powerFocus_>=0)
            BlendWhite(PowerMenuX,menu.top+powerFocus_*menu.row,menu.width,menu.row);
        Frame(PowerMenuX,menu.top,menu.width,2*menu.row,ACCENT);"""))

patch("xdos_lvgl.cpp",
    ("    if (!desktopCanvas_ || lv_screen_active() != homeScreen_ ||\n        !desktop_->NeedsRedraw()) return;",
     """    if (!desktopCanvas_ || lv_screen_active() != homeScreen_) return;
    if (AtDesktop()) {
        bool pointerFound=false;
        for (auto *input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input)) {
            if (lv_indev_get_type(input)!=LV_INDEV_TYPE_POINTER) continue;
            lv_point_t p;
            lv_indev_get_point(input,&p);
            desktop_->UpdateDesktopPointer(p.x,p.y);
            pointerFound=true;
            break;
        }
        if (!pointerFound) desktop_->UpdateDesktopPointer(-1,-1);
    }
    if (!desktop_->NeedsRedraw()) return;"""))

patch("kernel.h",
    ("    static void KeyHandler(const char *text);",
     """    static void KeyHandler(const char *text);
    static void DesktopModifierHandler(unsigned char mods,const unsigned char keys[6]);
    bool desktopWindowsHeld_ = false;"""))

patch("kernel.cpp",
    ("void CKernel::KeyHandler(const char *text)",
     """void CKernel::DesktopModifierHandler(unsigned char mods,const unsigned char[6])
{
    if (!instance_) return;
    // HID keyboard modifier bits 3 and 7 are left and right GUI.
    const bool pressed=(mods&0x88)!=0;
    if (pressed && !instance_->desktopWindowsHeld_) instance_->QueueKey(XdosKeyWindows);
    instance_->desktopWindowsHeld_=pressed;
}

void CKernel::KeyHandler(const char *text)"""),
    ("            kbd->RegisterKeyPressedHandler(KeyHandler);",
     """            desktopWindowsHeld_=false;
            kbd->RegisterKeyStatusHandlerRaw(DesktopModifierHandler,TRUE);
            kbd->RegisterKeyPressedHandler(KeyHandler);"""),
    ("        if (ui_.OnKey(key)) continue;", """        if (ui_.OnKey(key)) continue;
        if (desktop_.DesktopNavigationKey(key)) continue;"""))

keys = root / "xdos_text_keys.h"
source = keys.read_text(encoding="utf-8")
end = source.index("};")
keys.write_text(source[:end].rstrip().rstrip(",") + ",\n    XdosKeyWindows\n" + source[end:], encoding="utf-8")

patch("xdos_lvgl.h",
    ("    bool CloseCurrentApp();", """    bool CloseCurrentApp();
    bool HandleTaskKey(unsigned key);"""),
    ("    bool windowClosing_ = false;", """    bool windowClosing_ = false;
    lv_obj_t *appTask_ = nullptr;
    lv_obj_t *appWindowBody_ = nullptr;
    bool appTaskFocused_ = false;
    bool appMinimized_ = false;
    bool appMaximized_ = false;
    int appRestoreX_ = 0, appRestoreY_ = 0, appRestoreW_ = 0, appRestoreH_ = 0;
    void ShowAppTask(const char *title);
    void ClearAppTask();
    void FocusAppTask(bool focused);
    void ToggleAppMaximized();
    void RestoreAppTask();
    static void AppTaskPressed(lv_event_t *event);
    static void AppMinimizePressed(lv_event_t *event);
    static void AppMaximizePressed(lv_event_t *event);"""))

patch("xdos_lvgl.cpp",
    ("    lv_obj_align(demoBack_, LV_ALIGN_RIGHT_MID, -6, 0);",
     """    lv_obj_align(demoBack_, LV_ALIGN_RIGHT_MID, -6, 0);
    auto *maximize=MakeWindowButton(bar,"[]",AppMaximizePressed,this);
    lv_obj_set_size(maximize,29,25);
    lv_obj_align(maximize,LV_ALIGN_RIGHT_MID,-39,0);
    auto *minimize=MakeWindowButton(bar,"_",AppMinimizePressed,this);
    lv_obj_set_size(minimize,29,25);
    lv_obj_align(minimize,LV_ALIGN_RIGHT_MID,-72,0);"""),
    ("    lv_obj_set_size(body, width-2, height-37);", """    lv_obj_set_size(body, width-2, height-37);
    appWindowBody_=body;
    ShowAppTask(title);"""),
    ("    RestoreWindowTimers();\n    lv_obj_delete_async(demoScreen_);",
     """    RestoreWindowTimers();
    ClearAppTask();
    lv_obj_delete_async(demoScreen_);"""),
    ("    if (!desktopCanvas_ || lv_screen_active() != homeScreen_) return;",
     """    if (!demoScreen_ && appTask_) ClearAppTask();
    if (!desktopCanvas_ || lv_screen_active() != homeScreen_) return;"""),
    ("    demoBack_ = MakeButton(lv_layer_top(),\"BACK TO DESKTOP\",DemoBack,this);",
     """    ShowAppTask(measurements ? "LVGL Benchmark" : "LVGL Demo");
    demoBack_ = MakeButton(lv_layer_top(),"BACK TO DESKTOP",DemoBack,this);"""))

with (root / "xdos_lvgl.cpp").open("a", encoding="utf-8") as out:
    out.write(r'''
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
''')

patch("xdos_lvgl.cpp",
    ("    appWindowBody_=body;\n    ShowAppTask(title);", "    ShowAppTask(title);\n    appWindowBody_=body;"))

patch("kernel.h",
    ("    bool desktopWindowsHeld_ = false;", "    bool desktopWindowsHeld_ = false;\n    bool desktopAltF4Held_ = false;"))

patch("kernel.cpp",
    ("void CKernel::DesktopModifierHandler(unsigned char mods,const unsigned char[6])",
     "void CKernel::DesktopModifierHandler(unsigned char mods,const unsigned char keys[6])"),
    ("    instance_->desktopWindowsHeld_=pressed;", """    instance_->desktopWindowsHeld_=pressed;
    bool altF4=false;
    if (mods&0x44) {
        for (unsigned i=0;i<6;++i) if (keys[i]==0x3D) altF4=true;
    }
    if (altF4 && !instance_->desktopAltF4Held_) instance_->QueueKey(XdosKeyAltF4);
    instance_->desktopAltF4Held_=altF4;"""),
    ("            desktopWindowsHeld_=false;", "            desktopWindowsHeld_=false;\n            desktopAltF4Held_=false;"),
    ("        if (ui_.OnKey(key)) continue;", "        if (ui_.HandleTaskKey(key) || ui_.OnKey(key)) continue;"))

keys.write_text(keys.read_text(encoding="utf-8").replace(
    "    XdosKeyWindows\n", "    XdosKeyWindows, XdosKeyAltF4\n"),encoding="utf-8")

patch("desktop.cpp",
    ('    Text(width_-178,height_-30,"Pi 3B  ARM32",CYAN);\n', ""))
patch("xdos_lvgl.cpp",
    ("        lv_obj_align(desktopMonitor_,LV_ALIGN_TOP_RIGHT,-12,8);",
     "        lv_obj_align(desktopMonitor_,LV_ALIGN_BOTTOM_RIGHT,-14,-16);"))

patch("xdos_lvgl.cpp",
    ("    const int desktopHeight = desktop_->Height();\n    demoScreen_ = lv_obj_create(homeScreen_);",
     """    const int desktopHeight = desktop_->Height();
    height=LV_MIN(height+22,desktopHeight-49);
    demoScreen_ = lv_obj_create(homeScreen_);"""),
    ("    lv_obj_set_size(body, width-2, height-37);\n    ShowAppTask(title);",
     """    lv_obj_set_size(body, width-2, height-59);
    auto *hint=lv_label_create(window);
    lv_label_set_text(hint,"Alt+F4  Close XAPP");
    lv_obj_set_style_text_color(hint,lv_color_hex(0x294A6A),0);
    lv_obj_align(hint,LV_ALIGN_BOTTOM_LEFT,9,-3);
    ShowAppTask(title);"""))

patch("desktop_apps.h",
    ("constexpr unsigned XdosDesktopAppLimit = 10;",
     """constexpr unsigned XdosDesktopColumns = 6;
constexpr unsigned XdosDesktopAppLimit = XdosDesktopColumns * 2;"""))

patch("desktop.cpp",
    ('#include "pixel_bbs.h"', '#include "pixel_bbs.h"\n#include "desktop_apps.h"'),
    ("    constexpr int columns=5;", "    constexpr int columns=XdosDesktopColumns;"),
    ("    else if (key==XdosKeyLeft && index%5>0) --index;", "    else if (key==XdosKeyLeft && unsigned(index)%XdosDesktopColumns>0) --index;"),
    ("    else if (key==XdosKeyRight && index%5<4 && unsigned(index+1)<discoveredCount_) ++index;",
     "    else if (key==XdosKeyRight && unsigned(index)%XdosDesktopColumns<XdosDesktopColumns-1 && unsigned(index+1)<discoveredCount_) ++index;"),
    ("    else if (key==XdosKeyUp && index>=5) index-=5;", "    else if (key==XdosKeyUp && unsigned(index)>=XdosDesktopColumns) index-=XdosDesktopColumns;"),
    ("    else if (key==XdosKeyDown && index<5 && discoveredCount_>5)\n        index=Clamp(index+5,5,int(discoveredCount_)-1);",
     """    else if (key==XdosKeyDown && unsigned(index)<XdosDesktopColumns && discoveredCount_>XdosDesktopColumns)
        index=Clamp(index+XdosDesktopColumns,XdosDesktopColumns,int(discoveredCount_)-1);"""))

patch("kernel.cpp",
    ('#include "xapp_loader.h"', '#include "xapp_loader.h"\n#include <circle/actled.h>') if (root/"kernel.cpp").read_text().count('#include "xapp_loader.h"')==1 else
    ('#include "xapp_keypad.h"', '#include "xapp_keypad.h"\n#include <circle/actled.h>'),
    ("extern \"C\" void xdos_present_frame();", """extern "C" void xdos_present_frame();
extern "C" void xdos_arduino_set_act_led(int on)
{
    if (on) CActLED::Get()->On();
    else CActLED::Get()->Off();
}"""),
    ("    desktop_.ScanDesktopApps();", """    f_rename("SD:/XDOS/INBOX/LEDBLINK.APP","SD:/XDOS/APPS/LEDBLINK.APP");
    f_rename("SD:/XDOS/INBOX/ARDLVGL.APP","SD:/XDOS/APPS/ARDLVGL.APP");
    desktop_.ScanDesktopApps();"""),
    ("                desktop_.ReportAppResult(result==XappLoaderV1::OK ?",
     """                if (arduino && result==XappLoaderV1::OK) ui_.SetAppTaskTitle(app->name);
                desktop_.ReportAppResult(result==XappLoaderV1::OK ?"""))

patch("xdos_lvgl.h", ("    bool HandleTaskKey(unsigned key);",
    "    bool HandleTaskKey(unsigned key);\n    void SetAppTaskTitle(const char *title);"))
with (root / "xdos_lvgl.cpp").open("a", encoding="utf-8") as out:
    out.write("""
void XDOSLVGL::SetAppTaskTitle(const char *title)
{
    if (appTask_) lv_label_set_text(lv_obj_get_child(appTask_,0),title);
}
""")
