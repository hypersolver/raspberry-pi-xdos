#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned upload_us;
    unsigned draw_us;
    unsigned swap_us;
} XdosVc4Timing;

int xdos_vc4_start(unsigned width, unsigned height);
int xdos_vc4_active(void);
int xdos_vc4_shift_scroll(int delta, unsigned right, unsigned top);
int xdos_vc4_shift_viewport(int delta,unsigned left,unsigned top,unsigned right,unsigned bottom);
int xdos_vc4_upload(unsigned x, unsigned y, unsigned width, unsigned height,
                    const void *rgb565, int last, XdosVc4Timing *timing);
void xdos_vc4_stop(void);
unsigned xdos_vc4_clock_us(void);

#ifdef __cplusplus
}
#endif
