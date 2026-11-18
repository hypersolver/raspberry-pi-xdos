#ifndef XDOS_ASM_GUI_H
#define XDOS_ASM_GUI_H
// AAPCS32: implemented in xdos_gui.S, not C/C++.
// Color input: RGB888 encoded as 0x00RRGGBB.
#ifdef __cplusplus
extern "C" {
#endif
void xdos_arm_rect(void *fb, unsigned pitch, int screenW, int screenH,
                   int x, int y, int w, int h, unsigned rgb, unsigned depth);
void xdos_arm_glyph(void *fb, unsigned pitch, int screenW, int screenH,
                    int x, int y, const unsigned short rows[16], unsigned rgb, unsigned depth);
int xdos_arm_hit_window(const void *win, int pointerX, int pointerY);
int xdos_arm_drag_window(void *win, int pointerX, int pointerY,
                         int dragOffsetX, int dragOffsetY, int maxX, int maxY);
#ifdef __cplusplus
}
#endif
#endif
