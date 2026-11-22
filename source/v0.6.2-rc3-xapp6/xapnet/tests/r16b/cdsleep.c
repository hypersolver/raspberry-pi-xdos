/* R16-B test: xdos_core_sleep_us() on a borrowed core.
 * The job sleeps 100 ms twenty times and measures each wait with xdos_core_time_us();
 * it returns the worst overshoot in microseconds (job result in XAPP2-STATUS:
 * "<core>=done(<us>)"), or -1 when a stop request ended a sleep early. Core 0 logs
 * "[CDSLEEP] ..." on the UART when the job ends. Close via "@XDOS XAPP2-CLOSE <slot>"
 * (requests stop, waits, reclaims, exits). */
#include <stdint.h>
#include "lvgl.h"
#include "xdos_sdk_v1.h"
#include "xdos_corejobs.h"

typedef struct {
    const XdosHostApiV1 *api;
    int slot;
    volatile int core;
    volatile int32_t worst_us;
    volatile int32_t total_ms;
    int reported, closing;
    lv_timer_t *timer;
} State;

static State st;

static int32_t job(void *arg)
{
    State *s = (State *)arg;
    int32_t worst = 0;
    const uint32_t begin = xdos_core_time_us();
    for (int i = 0; i < 20; ++i) {
        const uint32_t t0 = xdos_core_time_us();
        if (xdos_core_sleep_us(100000u) != 0) return -1;   /* stop requested */
        const int32_t over = (int32_t)(xdos_core_time_us() - t0) - 100000;
        if (over > worst) worst = over;
    }
    s->total_ms = (int32_t)((xdos_core_time_us() - begin) / 1000u);
    s->worst_us = worst;
    return worst;
}

static void tick(lv_timer_t *timer)
{
    State *s = (State *)lv_timer_get_user_data(timer);
    int32_t r = 0;
    const int state = s->core >= 0 ? xdos_core_job_state((unsigned)s->core, &r) : -2;
    if (state == 0 && !s->reported) {
        s->reported = 1;
        if (s->api && s->api->log) {
            char line[96];
            lv_snprintf(line, sizeof line, "[CDSLEEP] core %d: 20 x 100 ms in %d ms, worst overshoot %d us\r\n",
                        s->core, (int)s->total_ms, (int)r);
            s->api->log(s->api->context, line);
        }
    }
    if (s->closing && (state == 0 || state == -2)) {
        if (s->core >= 0) xdos_core_reclaim((unsigned)s->core);
        if (s->api && s->api->log)
            s->api->log(s->api->context, r == -1 ? "[CDSLEEP] stopped early by close\r\n" : "[CDSLEEP] closed\r\n");
        lv_timer_delete(s->timer);
        s->timer = 0;
        xdos_v2_exit(s->slot);
    }
}

static void on_close(void *ctx)
{
    State *s = (State *)ctx;
    s->closing = 1;
    if (s->core >= 0) xdos_core_request_stop((unsigned)s->core);
}

int32_t xapp_entry(const XdosHostApiV1 *api)
{
    State *s = &st;
    s->api = api;
    s->slot = xdos_v2_slot_of((const void *)xapp_entry);
    if (s->slot < 0) return -1;
    xdos_v2_register_close(s->slot, on_close, s);
    s->core = xdos_core_lend_any(job, s);
    if (s->core < 0) {
        if (api && api->log) api->log(api->context, "[CDSLEEP] no core may be lent\r\n");
        s->closing = 1;
    }
    s->timer = lv_timer_create(tick, 50, s);
    if (api && api->log && s->core >= 0) api->log(api->context, "[CDSLEEP] job started on a borrowed core\r\n");
    return 0;
}
