"""Remove the Update XAPP from generated firmware, keeping UART deployment."""

from pathlib import Path
import re
import sys


source = Path(sys.argv[1])


def edit(name, *changes):
    path = source / name
    text = path.read_text(encoding="utf-8")
    for pattern, replacement in changes:
        text, count = re.subn(pattern, replacement, text, flags=re.MULTILINE)
        if not count:
            raise RuntimeError(f"{name}: missing Update XAPP anchor {pattern!r}")
    path.write_text(text, encoding="utf-8")


edit("desktop.cpp",
     (r"^    if\(Contains[^\n]+workspacePending_=UpdateApp;[^\n]+\n", ""),
     (r'^    Fill\(433,shortcutY[^\n]+\n    Frame\(433,shortcutY[^\n]+\n'
      r'    Text\(446,shortcutY[^\n]+\n    Text\(422,shortcutY[^\n]+\n', ""))
edit("desktop.h", (r", UpdateApp\b", ""),
     (r"^    bool BootServiceReady\(\)[^\n]+\n", ""))
edit("app_shortcut.cpp", (r'^        \{"SD:/XDOS/APPS/UPDATE\.APP"[^\n]+\n', ""))
edit("uart_service.h", (r"Browser, Update, Widgets", "Browser, Widgets"),
     (r', \{"UPDATE",Shortcut::Update\}', ""))
edit("kernel.cpp", (r"^            case Shortcut::Update:[^\n]+\n", ""),
     (r"^                            case XDOS_REQUEST_SELECT_IMAGE:[\s\S]+?"
      r"(?=^                            default: return -1;)", ""))
edit("xdos_lvgl.cpp",
     (r"\(\(key == 10 \|\| key == 13\) && workspacePanel_ != XDOS_PANEL_UPDATE\)",
      "(key == 10 || key == 13)"),
     (r'\(workspacePanel_ == XDOS_PANEL_UPDATE \? "[^"]+" : "Request accepted"\)',
      '"Request accepted"'),
     (r"^    if \(workspacePanel_ == XDOS_PANEL_UPDATE\) return;\n", ""),
     (r"panel > XDOS_PANEL_UPDATE", "panel > XDOS_PANEL_HTTP"),
     (r"^    const bool windowed = panel != XDOS_PANEL_UPDATE;\n", ""),
     (r"    if \(windowed && \(width < 270 \|\| height < 205\)\) return -1;[\s\S]+?"
      r"(?=^    workspaceInput_ =)", """    if (width < 270 || height < 205) return -1;
    auto *body = OpenAppWindow(title,width,height);
    if (!body) return -1;
"""),
     (r"windowed \? MakeWindowTextArea\(body\) : lv_textarea_create\(body\)",
      "MakeWindowTextArea(body)"),
     (r"windowed \? (MakeWindowButton\(body,label,WorkspaceButton,this\)) :\s+"
      r"MakeButton\(body,label,WorkspaceButton,this\)", r"\1"),
     (r"windowed \? (LV_MIN\(146,\(width-24\)/3\)) : 146", r"\1"),
     (r"windowed \? (8|51|93|119|height-166) : (48|94|142|168|lv_pct\(68\))", r"\1"),
     (r"^    if \(!windowed\) \{\n[\s\S]+?^    \}\n", ""),
     (r"^    if \(!windowed\) lv_screen_load\(demoScreen_\);\n", ""),
     (r"^    if \(panel == XDOS_PANEL_UPDATE\) \{\n[\s\S]+?^    \}\n", ""))
