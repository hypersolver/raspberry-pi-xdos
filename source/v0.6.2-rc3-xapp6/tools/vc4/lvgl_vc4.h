#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned upload_us;
    unsigned draw_us;
    unsigned swap_us;
    unsigned uploaded_bytes;
} XdosVc4Timing;

int xdos_vc4_start(unsigned width, unsigned height);
int xdos_vc4_active(void);
void xdos_vc4_get_geometry(unsigned *width,unsigned *height,
                           unsigned *hdmi_width,unsigned *hdmi_height);
int xdos_vc4_shift_scroll(int delta, unsigned right, unsigned top);
int xdos_vc4_shift_viewport(int delta,unsigned left,unsigned top,unsigned right,unsigned bottom);
void xdos_vc4_track_uploads(int enabled);
void xdos_vc4_diff_frame(void);
int xdos_vc4_upload(unsigned x, unsigned y, unsigned width, unsigned height,
                    const void *rgb565, int last, XdosVc4Timing *timing);
void xdos_vc4_stop(void);
unsigned xdos_vc4_clock_us(void);

#ifdef __cplusplus
}
#endif
