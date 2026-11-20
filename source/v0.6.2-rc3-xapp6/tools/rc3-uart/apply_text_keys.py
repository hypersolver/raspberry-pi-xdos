"""Decode Circle's special keys before passing text into LVGL fields."""

from pathlib import Path
import sys


source, circle = map(Path, sys.argv[1:])


def edit(path, old, new):
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"{path.name}: expected one keyboard anchor: {old!r}")
    path.write_text(text.replace(old, new), encoding="utf-8")


header = source / "xdos_text_keys.h"
header.write_text("""#pragma once
enum XdosTextKey : unsigned {
    XdosKeyLeft = 256, XdosKeyRight, XdosKeyUp, XdosKeyDown,
    XdosKeyHome, XdosKeyEnd, XdosKeyDelete, XdosKeyInsert
};
""", encoding="utf-8")

edit(circle / "lib/koptions.cpp", "strcpy (m_KeyMap, DEFAULT_KEYMAP);",
     'strcpy (m_KeyMap, "US");')
edit(source / "kernel.h", "void QueueKey(char ch);", "void QueueKey(unsigned key);")
edit(source / "kernel.h", "volatile char keys_[QUEUE];", "volatile unsigned keys_[QUEUE];")
edit(source / "kernel.cpp", "#include \"uart_service.h\"",
     '#include "uart_service.h"\n#include "xdos_text_keys.h"')
edit(source / "kernel.cpp", "void CKernel::QueueKey(char ch)", "void CKernel::QueueKey(unsigned key)")
edit(source / "kernel.cpp", "keys_[h]=ch;", "keys_[h]=key;")
edit(source / "kernel.cpp", "    while(*text)instance_->QueueKey(*text++);", r"""    if (*text == '\x1b' && text[1]) {
        static const struct { const char *sequence; XdosTextKey key; } keys[] = {
            {"\x1b[D",XdosKeyLeft}, {"\x1b[C",XdosKeyRight},
            {"\x1b[A",XdosKeyUp}, {"\x1b[B",XdosKeyDown},
            {"\x1b[1~",XdosKeyHome}, {"\x1b[4~",XdosKeyEnd},
            {"\x1b[3~",XdosKeyDelete}, {"\x1b[2~",XdosKeyInsert},
        };
        for (const auto &key : keys) {
            if (!strcmp(text,key.sequence)) {
                instance_->QueueKey(key.key);
                break;
            }
        }
        return;
    }
    while (*text) instance_->QueueKey(static_cast<unsigned char>(*text++));""")
edit(source / "kernel.cpp", "unsigned t=keyTail_;char ch=keys_[t];",
     "unsigned t=keyTail_;const unsigned key=keys_[t];")
edit(source / "kernel.cpp", "if (ui_.OnKey(ch)) continue;", "if (ui_.OnKey(key)) continue;")
edit(source / "kernel.cpp", "        desktop_.Key(ch);",
     "        if (key < 256) desktop_.Key(static_cast<char>(key));")

edit(source / "xdos_lvgl.h", "bool OnKey(char ch);", "bool OnKey(unsigned key);")
edit(source / "xdos_lvgl.h", "    void SuspendMonitor();", """    void SuspendMonitor();
    void EditText(lv_obj_t *field, unsigned key);
    bool overwriteText_ = false;""")
edit(source / "xdos_lvgl.cpp", '#include "xdos_lvgl.h"',
     '#include "xdos_lvgl.h"\n#include "xdos_text_keys.h"')
edit(source / "xdos_lvgl.cpp", "    SuspendMonitor();\n    return body;",
     "    overwriteText_ = false;\n    SuspendMonitor();\n    return body;")
edit(source / "xdos_lvgl.cpp", """bool XDOSLVGL::OnKey(char ch)
{""", """namespace {
unsigned LineEdge(lv_obj_t *field, bool end)
{
    const unsigned cursor = lv_textarea_get_cursor_pos(field);
    unsigned position = 0, start = 0;
    for (auto *text = lv_textarea_get_text(field); *text; ++text) {
        if ((static_cast<unsigned char>(*text) & 0xC0) == 0x80) continue;
        if (*text == '\\n') {
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
{""")
edit(source / "xdos_lvgl.cpp", """        const unsigned key = static_cast<unsigned char>(ch);
        if (key == 19)""", "        if (key == 19)")
edit(source / "xdos_lvgl.cpp", r"""            if (key == 8 || key == 127) lv_textarea_delete_char(editorText_);
            else if (strlen(lv_textarea_get_text(editorText_)) < 2047) {
                if (key == 10 || key == 13) lv_textarea_add_char(editorText_,'\n');
                else if (key >= 32 && key <= 126) lv_textarea_add_char(editorText_,key);
            }""", r"""            if (key == 10 || key == 13) lv_textarea_add_char(editorText_,'\n');
            else EditText(editorText_,key);""")
edit(source / "xdos_lvgl.cpp", """        const unsigned key = static_cast<unsigned char>(ch);
        if (key == 8 || key == 127) lv_textarea_delete_char(workspaceInput_);
        else if (key == 10 || key == 13)
            WorkspaceCommand(workspacePanel_ == XDOS_PANEL_FILES ? 2 : 0);
        else if (key >= 32 && key <= 126) lv_textarea_add_char(workspaceInput_,key);""",
     """        if (key == 10 || key == 13)
            WorkspaceCommand(workspacePanel_ == XDOS_PANEL_FILES ? 2 : 0);
        else EditText(workspaceInput_,key);""")
edit(source / "xdos_lvgl.cpp", "    KeypadKey(static_cast<unsigned char>(ch));",
     "    if (key < 256) KeypadKey(key);")
