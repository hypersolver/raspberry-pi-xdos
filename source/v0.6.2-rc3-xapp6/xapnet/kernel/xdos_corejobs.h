/* XD OS V2 app runtime API (C2 steps 3-5). Plain C, AAPCS32.
 *
 * Execution model
 *   - A V2 app's entry, LVGL callbacks and lv_timers run on core 0 (the only
 *     LVGL / FatFs / network caller).
 *   - An app may borrow one core (1..3) for computation with
 *     xdos_core_lend_any(), within the lending limit below.
 *   - Code on a borrowed core must NOT call LVGL, FatFs, network or Circle
 *     directly. Use xdos_call_core0() to run a short function on core 0.
 *   - Clean-up (COLLAB C2): on close, request stop, wait until the job is
 *     finished (xdos_core_job_state()==0), xdos_core_reclaim(), delete own
 *     LVGL objects/timers, then xdos_v2_exit(slot). The kernel releases the
 *     slot on its next main-loop pass, after all callbacks have returned.
 *
 * All functions are kernel exports bound at pack time (like lv_* imports).
 */
#ifndef XDOS_COREJOBS_H
#define XDOS_COREJOBS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

int xdos_this_core(void);                                    /* 0..3 */

/* Core 0 only. Returns the borrowed core (3, 2 or 1) or -1 when none may be lent.
 * job(arg) starts on that core at its next idle point; its return value is
 * reported by xdos_core_job_state(). */
int xdos_core_lend_any(int32_t (*job)(void *arg), void *arg);
int xdos_core_lend(unsigned core, int32_t (*job)(void *arg), void *arg);   /* 1 ok */

/* 1 pending/running, 0 finished (*result set), -2 not lent, -1 bad core. */
int xdos_core_job_state(unsigned core, int32_t *result);
void xdos_core_request_stop(unsigned core);                  /* any core */
int xdos_core_stop_requested(void);                          /* on the borrowed core */
int xdos_core_reclaim(unsigned core);                        /* core 0; 1 when returned to LVGL */

/* Lending policy (R16-B). XDOS lends at most 3 cores when no full-screen app runs
 * and 2 while one does (a draw core stays with LVGL); xdos_core_lend_any() returns
 * -1 above that limit and the app then runs its work on core 0 (shared mode). */
unsigned xdos_core_lent_count(void);
unsigned xdos_core_lend_limit(void);

/* Timing on any core. xdos_core_time_us(): system timer in microseconds (wraps).
 * xdos_core_sleep_us(): on a borrowed core, wait without the Circle scheduler
 * (WFE + timer event stream, ~0.43 ms granularity); returns 0 after the full time,
 * 1 as soon as a stop was requested (end the job), -1 when called on core 0. */
uint32_t xdos_core_time_us(void);
int xdos_core_sleep_us(uint32_t us);

/* Core 0 shared mode budget (R16-B). An app without a borrowed core runs its work in
 * slices from lv_timer callbacks on core 0. Wrap each slice:
 *     if (xdos_v2_shared_begin(slot)) { ...slice...; xdos_v2_shared_end(slot); }
 * Aim for <= 2 ms per slice. After 3 slices in a row longer than 10 ms the runtime
 * throttles the app (begin() returns 0 for 3 of every 4 calls) and tells the user;
 * it is never killed. xdos_v2_shared_stats() returns slices over 2 ms so far. */
int xdos_v2_shared_begin(int slot);
void xdos_v2_shared_end(int slot);
unsigned xdos_v2_shared_stats(int slot);

/* Run fn(arg) on core 0 between LVGL frames and wait for it. Direct call when
 * already on core 0. Keep fn short: core 0 drives the UI. */
int xdos_call_core0(void (*fn)(void *arg), void *arg);

/* Slot lifecycle (core 0). */
int xdos_v2_slot_of(const void *address);                    /* slot of an address, or -1 */
void xdos_v2_register_close(int slot, void (*on_close)(void *ctx), void *ctx);
void xdos_v2_exit(int slot);                                 /* release after callbacks return */

/* Windows (core 0). xdos_v2_window() creates a movable window on the top layer
 * with a title bar and a close button (calls on_close) and returns its content
 * area (lv_obj_t *). The runtime deletes the window when the slot is released,
 * so the app must not delete it. Clicking a window focuses it; keys then go to
 * the handler registered with xdos_v2_register_key(): printable ASCII 32..126,
 * Enter 10 or 13, Backspace 8 or 127, Esc 27; 256 Left, 257 Right, 258 Up,
 * 259 Down, 260 Home, 261 End, 262 Delete, 263 Insert. Alt+F4 closes the
 * focused app (on_close); the Windows key stays with the desktop. */
struct _lv_obj_t;
struct _lv_obj_t *xdos_v2_window(int slot, const char *title, int width, int height);
void xdos_v2_register_key(int slot, void (*on_key)(void *ctx, unsigned key), void *ctx);

#ifdef __cplusplus
}
#endif
#endif
