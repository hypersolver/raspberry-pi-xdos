/*
 * XD OS AArch32 GUI primitives, translated from the concepts and control flow
 * of eklavya-raj/xdos kernel/kernel.asm (MIT): rect, glyph, dragging, hit-test.
 *
 * Input / output follow AAPCS32; called from the Circle C++ hardware shell.
 * Pi 3B, Cortex-A53, AArch32 ARM state. Freestanding, no Linux dependencies.
 *
 * 2D framebuffer addressing uses pitchBytes, NOT logical width*bytes/pixel.
 * Framebuffer color is supplied as 0x00RRGGBB (RGB888).
 * Supports 16-bit RGB565 and 32-bit XRGB8888.
 */
.syntax unified
.arch armv7-a
.arm
.text
.align 2

/* void xdos_arm_rect(void *fb, unsigned pitch, int screenW, int screenH,
 *                    int x, int y, int w, int h, unsigned rgb, unsigned depth);
 *
 * x86 source analogue: rect/clip (kernel.asm, original lines 925-974):
 *   clip -> row loop -> repeated stores (rep stosd).
 * ARM port: clip -> row loop -> STRH or STR, honoring hardware pitch.
 */
.global xdos_arm_rect
.type xdos_arm_rect,%function
xdos_arm_rect:
    push    {r4-r11,lr}
    ldr     r4, [sp,#36]          @ x
    ldr     r5, [sp,#40]          @ y
    ldr     r6, [sp,#44]          @ rect width
    ldr     r7, [sp,#48]          @ rect height
    ldr     r8, [sp,#52]          @ 0x00RRGGBB
    ldr     r9, [sp,#56]          @ depth
    cmp     r6, #0
    ble     .Lrect_return
    cmp     r7, #0
    ble     .Lrect_return
    cmp     r2, #0
    ble     .Lrect_return
    cmp     r3, #0
    ble     .Lrect_return
    cmp     r9, #16
    beq     .Lrect_clip
    cmp     r9, #32
    bne     .Lrect_return
.Lrect_clip:
    mov     r10, r4               @ left
    mov     r11, r5               @ top
    add     r6, r6, r4            @ right (exclusive)
    add     r7, r7, r5            @ bottom (exclusive)
    cmp     r10, #0
    movlt   r10, #0
    cmp     r11, #0
    movlt   r11, #0
    cmp     r6, r2
    movgt   r6, r2
    cmp     r7, r3
    movgt   r7, r3
    cmp     r10, r6
    bge     .Lrect_return
    cmp     r11, r7
    bge     .Lrect_return

    cmp     r9, #16
    beq     .Lrect_rgb565
    mov     r12, #0xff000000
    orr     r8, r8, r12
    b       .Lrect_row32
.Lrect_rgb565:
    and     r4, r8, #0x00f80000
    lsr     r4, r4, #8
    and     r5, r8, #0x0000fc00
    lsr     r5, r5, #5
    orr     r4, r4, r5
    and     r5, r8, #0x000000f8
    lsr     r5, r5, #3
    orr     r8, r4, r5
.Lrect_row16:
    mla     r12, r11, r1, r0      @ fb + row*pitch
    add     r12, r12, r10, lsl #1 @ + left*2
    mov     r4, r10
.Lrect_col16:
    strh    r8, [r12], #2
    add     r4, r4, #1
    cmp     r4, r6
    blt     .Lrect_col16
    add     r11, r11, #1
    cmp     r11, r7
    blt     .Lrect_row16
    b       .Lrect_return
.Lrect_row32:
    mla     r12, r11, r1, r0
    add     r12, r12, r10, lsl #2
    mov     r4, r10
.Lrect_col32:
    str     r8, [r12], #4
    add     r4, r4, #1
    cmp     r4, r6
    blt     .Lrect_col32
    add     r11, r11, #1
    cmp     r11, r7
    blt     .Lrect_row32
.Lrect_return:
    pop     {r4-r11,pc}
.size xdos_arm_rect, .-xdos_arm_rect

/* void xdos_arm_glyph(void *fb, unsigned pitch, int screenW, int screenH,
 *                     int x, int y, const unsigned short rows[16],
 *                     unsigned rgb, unsigned depth);
 *
 * x86 source analogue: glyph (kernel.asm lines 1183-1214):
 * read bitmap row -> test high bit -> draw pixel -> shift -> next row.
 * Font bitmap is provided in 16 8-bit masks, bit7 = leftmost pixel.
 */
.global xdos_arm_glyph
.type xdos_arm_glyph,%function
xdos_arm_glyph:
    push    {r4-r11,lr}
    ldr     r4, [sp,#36]          @ x
    ldr     r5, [sp,#40]          @ y
    ldr     r6, [sp,#44]          @ 16 packed row masks
    ldr     r7, [sp,#48]          @ RGB888
    ldr     r8, [sp,#52]          @ depth
    cmp     r8, #16
    beq     .Lglyph_rgb565
    cmp     r8, #32
    bne     .Lglyph_return
    mov     r12, #0xff000000
    orr     r7, r7, r12
    b       .Lglyph_start
.Lglyph_rgb565:
    and     r10, r7, #0x00f80000
    lsr     r10, r10, #8
    and     r11, r7, #0x0000fc00
    lsr     r11, r11, #5
    orr     r10, r10, r11
    and     r11, r7, #0x000000f8
    lsr     r11, r11, #3
    orr     r7, r10, r11
.Lglyph_start:
    mov     r9, #0                @ row index 0..15
.Lglyph_row:
    add     r12, r6, r9, lsl #1
    ldrh    r10, [r12]
    add     r11, r5, r9           @ screenY = y + row
    cmp     r11, #0
    blt     .Lglyph_next_row
    cmp     r11, r3
    bge     .Lglyph_next_row
    mla     lr, r11, r1, r0       @ framebuffer row pointer
    mov     r11, #0               @ col index 0..7
.Lglyph_col:
    tst     r10, #0x80           @ original x86 uses test cl,0x80
    beq     .Lglyph_next_col
    add     r12, r4, r11          @ screenX = x + col
    cmp     r12, #0
    blt     .Lglyph_next_col
    cmp     r12, r2
    bge     .Lglyph_next_col
    cmp     r8, #16
    bne     .Lglyph_write32
    add     r12, lr, r12, lsl #1
    strh    r7, [r12]
    b       .Lglyph_next_col
.Lglyph_write32:
    str     r7, [lr, r12, lsl #2]
.Lglyph_next_col:
    lsl     r10, r10, #1
    add     r11, r11, #1
    cmp     r11, #8
    blt     .Lglyph_col
.Lglyph_next_row:
    add     r9, r9, #1
    cmp     r9, #16
    blt     .Lglyph_row
.Lglyph_return:
    pop     {r4-r11,pc}
.size xdos_arm_glyph, .-xdos_arm_glyph

/* int xdos_arm_hit_window(const void *win, int px, int py);
 * Window offsets from desktop.h: x=0,y=4,w=8,h=12,open=16.
 * Port of x86 point-in-window comparisons (press hit testing).
 */
.global xdos_arm_hit_window
.type xdos_arm_hit_window,%function
xdos_arm_hit_window:
    ldrb    r3, [r0,#16]
    cmp     r3, #0
    beq     .Lhit_no
    ldr     r3, [r0,#0]
    cmp     r1, r3
    blt     .Lhit_no
    ldr     r12, [r0,#8]
    add     r3, r3, r12
    cmp     r1, r3
    bge     .Lhit_no
    ldr     r3, [r0,#4]
    cmp     r2, r3
    blt     .Lhit_no
    ldr     r12, [r0,#12]
    add     r3, r3, r12
    cmp     r2, r3
    bge     .Lhit_no
    mov     r0, #1
    bx      lr
.Lhit_no:
    mov     r0, #0
    bx      lr
.size xdos_arm_hit_window, .-xdos_arm_hit_window

/* int xdos_arm_drag_window(void *win, int px, int py, int dragDX,
 *                          int dragDY, int maxX, int maxY);
 * Return 1 if position updated, else 0.
 * x86 source analogue: dragging (kernel.asm 338-355): subtract click
 * offset and clamp window position; ARM translation adds screen bounds.
 */
.global xdos_arm_drag_window
.type xdos_arm_drag_window,%function
xdos_arm_drag_window:
    push    {r4-r7,lr}
    ldr     r4, [sp,#20]         @ dragDY
    ldr     r5, [sp,#24]         @ maxX
    ldr     r6, [sp,#28]         @ maxY
    sub     r3, r1, r3           @ new X = mouseX - dragDX
    sub     r4, r2, r4           @ new Y = mouseY - dragDY
    cmp     r5, #0
    movlt   r5, #0
    cmp     r6, #0
    movlt   r6, #0
    cmp     r3, #0
    movlt   r3, #0
    cmp     r4, #0
    movlt   r4, #0
    cmp     r3, r5
    movgt   r3, r5
    cmp     r4, r6
    movgt   r4, r6
    ldr     r7, [r0,#0]
    cmp     r7, r3
    bne     .Ldrag_changed
    ldr     r7, [r0,#4]
    cmp     r7, r4
    bne     .Ldrag_changed
    mov     r0, #0
    pop     {r4-r7,pc}
.Ldrag_changed:
    str     r3, [r0,#0]
    str     r4, [r0,#4]
    mov     r0, #1
    pop     {r4-r7,pc}
.size xdos_arm_drag_window, .-xdos_arm_drag_window
