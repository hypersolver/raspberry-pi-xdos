"""Add the VideoCore MP4 player to a generated ARM32 VC4 XDOS source tree."""
from pathlib import Path
import os
import shutil
import sys

source, deps = map(Path, sys.argv[1:3])
tools = Path(__file__).resolve().parent


def edit(name, old, new):
    path = source / name
    text = path.read_text()
    if text.count(old) != 1:
        raise RuntimeError(f"{name}: expected one {old!r}")
    path.write_text(text.replace(old, new))


for name in ("video_transport.h", "video_transport.cpp", "mp4_playback.inc", "player_host.inc"):
    shutil.copyfile(tools / name, source / name)
shutil.copyfile(deps / "minimp4/minimp4.h", source / "minimp4.h")
upstream = deps / "Pi1MHz/src/rpi"
for name in ("h264dec", "mmal_vc", "vcsm"):
    shutil.copyfile(upstream / f"{name}.h", source / f"{name}.h")
    text = (upstream / f"{name}.c").read_text()
    text = text.replace('#include "rpi.h"', '#include "video_transport.h"\n'
                        '#define RPI_GetSystemTime vc4_clock_us\n'
                        '#define LOG_INFO xdos_video_log\n'
                        '#define LOG_DEBUG(...)')
    text = text.replace('#include "vchiq.h"\n', '').replace('#include "systimer.h"\n', '')
    (source / f"{name}.c").write_text(text)
renderer = (tools / "video_playback.inc").read_text()
renderer = renderer[renderer.index("static GLuint video_shader"):renderer.index("static unsigned video_find_aud")]
renderer = renderer.replace("state->display", "display").replace("state->surface", "surface")
renderer = '#include "video_transport.h"\nstatic unsigned VideoWidth, VideoHeight, VideoFrameBytes;\n' + renderer
with (source / "lvgl_vc4.c").open("a") as file:
    optimized = int(os.environ.get("XDOS_PLAYER_OPTIMIZED", "1") == "1")
    file.write(renderer + f'\n#define XDOS_PLAYER_OPTIMIZED {optimized}\n#include "mp4_playback.inc"\n')
edit("lvgl_vc4.c", "static EGLDisplay display", "static EGLConfig config;\nstatic EGLDisplay display")
edit("lvgl_vc4.c", "    EGLConfig config;\n", "")
edit("Makefile", "OBJS += xdos_lvgl.o lvgl_vc4.o",
     "OBJS += xdos_lvgl.o lvgl_vc4.o video_transport.o h264dec.o mmal_vc.o vcsm.o")

edit("xapp_workspace.h", "XDOS_PANEL_TERMINAL = 4", "XDOS_PANEL_TERMINAL = 4, XDOS_PANEL_PLAYER = 5")
edit("xapp_workspace.h", "    XDOS_REQUEST_TERMINAL\n", "    XDOS_REQUEST_TERMINAL, XDOS_REQUEST_PLAY\n")
edit("desktop.h", "BrowserApp, TerminalApp", "BrowserApp, TerminalApp, PlayerApp")
edit("desktop.cpp", "    // Desktop shortcuts", """    if (Contains(x,y,465,97+NWIN*80,112,72)) {workspacePending_=PlayerApp;return;}
    // Desktop shortcuts""")
edit("desktop.cpp", '    Text(286,shortcutY+38,"LVGL BENCHMARK",WHITE);', """
    Text(286,shortcutY+38,"LVGL BENCHMARK",WHITE);
    Fill(474,shortcutY,34,32,0x4D7E71);
    Frame(474,shortcutY,34,32,0xC0D4EA);
    Text(487,shortcutY+8,"P",WHITE);
    Text(465,shortcutY+38,"MP4 PLAYER",WHITE);""")
edit("app_shortcut.cpp", "    if (app==4) {", """    if (app==5) {
        return XappLoaderV1::ExecuteFile("SD:/XDOS/INBOX/PLAYER.XAPP",api)==XappLoaderV1::OK
            ? "XAPP: Player started" : "XAPP: Player module missing or incompatible";
    }
    if (app==4) {""")

edit("uart_service.h", "Close, Terminal };", "Close, Terminal, Player, Play };")
edit("uart_service.h", "    bool TransferActive()", "    const char *PlayPath() const { return playPath_; }\n    bool TransferActive()")
edit("uart_service.h", "    Shortcut shortcut_ = Shortcut::None;",
     "    Shortcut shortcut_ = Shortcut::None;\n    char playPath_[192]{};")
edit("uart_service.h", '{"TERMINAL",Shortcut::Terminal}',
     '{"PLAYER",Shortcut::Player}, {"TERMINAL",Shortcut::Terminal}')
edit("uart_service.h", "BenchResult, Open, Close,", "BenchResult, Open, Play, Close,")
edit("uart_service.h", '        {Command::Open, "OPEN", true, false},',
     '        {Command::Open, "OPEN", true, false},\n        {Command::Play, "PLAY", true, false},')
edit("uart_service.h", "        if (command==Command::Open) {", r"""        if (command==Command::Play) {
            const char *path=cmd+5;
            if (mounted && !active_ && shortcut_==Shortcut::None && *path &&
                strlen(path)<sizeof playPath_) {
                strcpy(playPath_,path);
                shortcut_=Shortcut::Play;
            } else Reply(serial,"@XDOS ERROR\r\n");
            return;
        }
        if (command==Command::Open) {""")
edit("kernel.cpp", '#include <string.h>', '#include <string.h>\n#include <strings.h>\n#include "player_host.inc"')
edit("kernel.cpp", "            case Shortcut::Terminal: workspace=Desktop::TerminalApp; break;",
     """            case Shortcut::Terminal: workspace=Desktop::TerminalApp; break;
            case Shortcut::Player: case Shortcut::Play: workspace=Desktop::PlayerApp; break;""")
edit("kernel.cpp", "                            case XDOS_REQUEST_HTTP:",
     "                            case XDOS_REQUEST_PLAY: return QueueVideo(text) ? 0 : -1;\n"
     "                            case XDOS_REQUEST_HTTP:")
edit("kernel.cpp", "        if (shortcut==Shortcut::Close) uart.CompleteClose(serial_,ui_.CloseCurrentApp());", r"""
        if (shortcut==Shortcut::Play) {
            const bool accepted=canOpen && !ui_.AtDesktop() && QueueVideo(uart.PlayPath());
            if (!accepted && canOpen && !ui_.AtDesktop()) ui_.CloseCurrentApp();
            serial_.Write(accepted ? "@XDOS PLAYING\r\n" : "@XDOS ERROR\r\n",
                          accepted ? 15 : 13);
        }
        else if (shortcut==Shortcut::Close) uart.CompleteClose(serial_,ui_.CloseCurrentApp());""")
edit("kernel.cpp", "        ui_.Tick();", r"""        if (playerPath[0]) {
            const int result=RunQueuedVideo(debugSerialReady_ ? &serial_ : nullptr);
            desktop_.ReportAppResult(result ? "MP4 playback failed or unsupported" : "MP4 playback ended");
            if (debugSerialReady_) {
                const char *message=result ? "@XDOS PLAY ERROR\r\n" : "@XDOS PLAY DONE\r\n";
                serial_.Write(message,strlen(message));
            }
            desktop_.RequestFullRepaint();
        }
        ui_.Tick();""")

edit("xdos_lvgl.cpp", '#include "desktop.h"', '#include "desktop.h"\n#include <fatfs/ff.h>\n#include <strings.h>')
edit("xdos_lvgl.cpp", "    if (workspacePanel_ == XDOS_PANEL_TERMINAL) {",
     "    if (workspacePanel_ == XDOS_PANEL_PLAYER) return;\n    if (workspacePanel_ == XDOS_PANEL_TERMINAL) {")
edit("xdos_lvgl.cpp", "panel != XDOS_PANEL_TERMINAL)",
     "panel != XDOS_PANEL_TERMINAL && panel != XDOS_PANEL_PLAYER)")
edit("xdos_lvgl.cpp", "    if (panel == XDOS_PANEL_FILES) WorkspaceCommand(0);", r"""
    if (panel == XDOS_PANEL_PLAYER) {
        lv_obj_add_flag(workspaceOutput_,LV_OBJ_FLAG_HIDDEN);
        auto *picker=lv_dropdown_create(body);
        lv_obj_set_width(picker,lv_pct(94));
        lv_obj_align(picker,LV_ALIGN_TOP_MID,0,119);
        lv_dropdown_set_options(picker,"Choose an MP4 from SD");
        const char *folders[]={"SD:/","SD:/XDOS/INBOX/","SD:/XDOS/VIDEO/"};
        unsigned listed=0;
        for (const char *folder : folders) {
            DIR dir;
            if (f_opendir(&dir,folder)!=FR_OK) continue;
            FILINFO entry;
            while (listed<128 && f_readdir(&dir,&entry)==FR_OK && entry.fname[0]) {
                const size_t n=strlen(entry.fname);
                if ((entry.fattrib&AM_DIR) || n<5 || strcasecmp(entry.fname+n-4,".mp4") ||
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
    if (panel == XDOS_PANEL_FILES) WorkspaceCommand(0);""")
