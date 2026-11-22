/* R16-B test copy of the Core Demo (from CLAUD XDOS/tools/mc4/xappv2/demo):
 * - the borrowed-core job never blocks on core 0 (no xdos_call_core0), the 250 ms
 *   timer on core 0 shows progress;
 * - without a core it computes on core 0 in slices wrapped with
 *   xdos_v2_shared_begin()/end(); -DR16B_SLOW_SLICE makes each slice ~15 ms to
 *   exercise the runtime throttle.
 * CORE2: XAPP V2 demo for C2 steps 3-5.
 * Each instance (one per V2 slot) borrows a draw core (3, then 2) and counts
 * primes there; the borrowed core asks core 0 to refresh the window through
 * xdos_call_core0(). With no core left it runs cooperatively on core 0 in
 * lv_timer chunks. Close: button or "@XDOS XAPP2-CLOSE <slot>". */
#include <stdint.h>
#include "lvgl.h"
#include "xdos_sdk_v1.h"
#include "xdos_corejobs.h"
#ifdef XDOS_DEMO_MEMORY
#include "xdos_xapp_memory.h"
#endif

#define LIMIT 2000000u

typedef struct {
    const XdosHostApiV1 *api;
    int slot;
    volatile int core;          /* borrowed core, -1 cooperative, -2 returned */
    volatile uint32_t next;     /* next number to test */
    volatile uint32_t primes;
    volatile int32_t result;
    volatile int finished;
    volatile int run_core;      /* CPU the job really ran on (xdos_this_core) */
    volatile uint32_t rounds;   /* XDOS_DEMO_FOREVER: completed passes */
    int closing;
    uint32_t remote_updates;
    lv_obj_t *panel, *label;
    lv_timer_t *timer;
} State;

static State st;                /* .bss: one copy per slot */

static int is_prime(uint32_t n)
{
    if (n < 2) return 0;
    if (n % 2 == 0) return n == 2;
    for (uint32_t d = 3; d * d <= n; d += 2)
        if (n % d == 0) return 0;
    return 1;
}

static void show(State *s)
{
    if (!s->label) return;
    const char *where __attribute__((unused)) = s->core >= 0 ? "core" : s->core == -1 ? "core0 coop" : "done on core";
#ifdef XDOS_DEMO_FOREVER
    lv_label_set_text_fmt(s->label, "Core Demo  slot %d\nrunning on CPU %d (%s)\npass %u: %u%%  primes %u\nUI updates from CPU %d: %u",
                          s->slot, s->run_core, s->core >= 0 ? "borrowed core" : "core 0, shared",
                          (unsigned)s->rounds + 1, (unsigned)((uint64_t)s->next * 100u / LIMIT),
                          (unsigned)s->primes, s->run_core, (unsigned)s->remote_updates);
#else
    lv_label_set_text_fmt(s->label, "slot %d  %s %d (ran on CPU %d)\n%u primes < %u (%u%%)\nUI updates from core: %u",
                          s->slot, where, s->core >= 0 ? s->core : 0, s->run_core,
                          (unsigned)s->primes, (unsigned)s->next,
                          (unsigned)((uint64_t)s->next * 100u / LIMIT), (unsigned)s->remote_updates);
#endif
}

static void __attribute__((unused)) remote_show(void *arg)    /* runs on core 0, called from the borrowed core */
{
    State *s = (State *)arg;
    s->remote_updates++;
    show(s);
}

static int32_t __attribute__((unused)) job(void *arg)   /* runs on the borrowed core: no LVGL here */
{
    State *s = (State *)arg;
    s->run_core = xdos_this_core();
#ifdef XDOS_DEMO_TRIVIAL_JOB
    s->next = LIMIT;                  /* C4 isolation: no loop, no core-0 call */
    return 42;
#endif
    uint32_t count = 0, n = 2;
    for (; n < LIMIT; ++n) {
        if ((n & 0x1FFFFu) == 0) {
            if (xdos_core_stop_requested()) break;
            s->next = n;
            s->primes = count;
        }
        count += (uint32_t)is_prime(n);
#ifdef XDOS_DEMO_FOREVER
        if (n + 1 == LIMIT) { s->rounds++; n = 1; count = 0; }
#endif
    }
    s->next = n;
    s->primes = count;
    return (int32_t)count;
}

static void finish(State *s)
{
    lv_timer_t *t = s->timer;
    s->timer = 0;
    if (s->panel) lv_obj_delete(s->panel);
    s->panel = s->label = 0;
    if (t) lv_timer_delete(t);
    xdos_v2_exit(s->slot);            /* kernel frees the slot after we return */
}

static void tick(lv_timer_t *timer)
{
    State *s = (State *)lv_timer_get_user_data(timer);
    if (s->core >= 0) {
        int32_t r = 0;
        if (xdos_core_job_state((unsigned)s->core, &r) == 0 && xdos_core_reclaim((unsigned)s->core)) {
            s->result = r;
            s->finished = 1;
            s->core = -2;
        }
    } else if (s->core == -1 && !s->finished) {
        if (!xdos_v2_shared_begin(s->slot)) { show(s); return; }   /* throttled: skip */
        uint32_t n = s->next, count = s->primes;
#ifdef R16B_SLOW_SLICE
        const uint32_t t0 = xdos_core_time_us();
        while (xdos_core_time_us() - t0 < 15000u && n < LIMIT && !s->closing) count += (uint32_t)is_prime(n++);
#else
        for (uint32_t i = 0; i < 4000 && n < LIMIT && !s->closing; ++i, ++n) count += (uint32_t)is_prime(n);
#endif
        s->next = n;
        s->primes = count;
#ifdef XDOS_DEMO_FOREVER
        if (n >= LIMIT) { s->rounds++; n = 2; count = 0; s->next = n; s->primes = 0; }
        if (s->closing) s->finished = 1;
#else
        if (n >= LIMIT || s->closing) s->finished = 1;
#endif
        xdos_v2_shared_end(s->slot);
    }
    if (s->closing && (s->finished || s->core == -2)) {
        finish(s);
        return;
    }
    show(s);
}

static void on_close(void *ctx)
{
    State *s = (State *)ctx;
    s->closing = 1;
    if (s->core >= 0) xdos_core_request_stop((unsigned)s->core);
}

static void close_clicked(lv_event_t *e)
{
    on_close(lv_event_get_user_data(e));
}

int32_t xapp_entry(const XdosHostApiV1 *api)
{
    State *s = &st;
    s->api = api;
    s->slot = xdos_v2_slot_of((const void *)xapp_entry);
    if (s->slot < 0) return -1;
#ifdef XDOS_DEMO_MEMORY
    const size_t quota = xdos_v2_memory_limit();
    unsigned char *buffer = quota ? xdos_v2_alloc(s->slot, quota - 64) : 0;
    if (!buffer || xdos_v2_alloc(s->slot, 4096)) {
        if (api && api->log) api->log(api->context, "[XAPP MEM] FAIL\r\n");
        return -1;
    }
    buffer[0] = 0xa5;
    if (api && api->log) api->log(api->context, "[XAPP MEM] ALLOC OK OVERQUOTA REJECTED\r\n");
    /* Deliberately leave the block allocated to test automatic reclamation. */
#endif
    s->next = 2;
    s->run_core = xdos_this_core();

    s->panel = lv_obj_create(lv_layer_top());
#ifdef XDOS_DEMO_FOREVER
    lv_obj_set_size(s->panel, 400, 150);
    lv_obj_align(s->panel, LV_ALIGN_TOP_RIGHT, -12, 40 + s->slot * 160);
#else
    lv_obj_set_size(s->panel, 380, 130);
    lv_obj_align(s->panel, LV_ALIGN_TOP_RIGHT, -12, 60 + s->slot * 140);
#endif
    s->label = lv_label_create(s->panel);
    lv_obj_align(s->label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *button = lv_button_create(s->panel);
    lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t *text = lv_label_create(button);
    lv_label_set_text(text, "Close");
    lv_obj_add_event_cb(button, close_clicked, LV_EVENT_CLICKED, s);

    xdos_v2_register_close(s->slot, on_close, s);
#ifdef XDOS_DEMO_NO_LEND
    s->core = -1;                                    /* C4 isolation: core 0 only */
#else
    s->core = xdos_core_lend_any(job, s);            /* -1: no free core */
#endif
    s->timer = lv_timer_create(tick, 250, s);
    show(s);
    if (api && api->log) api->log(api->context, s->core >= 0 ?
        "[CORE2] job started on a borrowed core\r\n" : "[CORE2] no free core, cooperative on core 0\r\n");
    return 0;
}
