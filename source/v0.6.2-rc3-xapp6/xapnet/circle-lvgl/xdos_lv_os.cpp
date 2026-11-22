// LV_OS_CUSTOM implementation for XDOS on Circle (bare metal, Pi 3, ARM32).
//
// * lv_thread_init() pins each LVGL software draw unit to its own core (1..3)
//   via CMultiCoreSupport. Those cores never run the Circle scheduler, take no
//   interrupts and only execute LVGL's render_thread_cb loop.
// * Waiting on core 0 Yields to other Circle tasks (USB, network, UART);
//   waiting on cores 1..3 uses WFE and is woken by SEV.
// * Mutexes are recursive spin locks keyed by core number. Only the main LVGL
//   task touches LVGL on core 0, so per-core ownership is sufficient.

#include <circle/multicore.h>
#include <circle/memory.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>
#include <circle/sched/synchronizationevent.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/synchronize.h>
#include <stdint.h>
#include <lvgl/lvgl.h>
#include "lvgl/src/osal/lv_os_private.h"

#ifndef ARM_ALLOW_MULTI_CORE
#error "XDOS multicore LVGL needs ARM_ALLOW_MULTI_CORE"
#endif

extern "C" volatile unsigned xdos_wfi_microseconds;   // Circle scheduler WFI patch
extern "C" volatile unsigned xdos_prof_irq_us, xdos_prof_irq_n, xdos_prof_fiq_us, xdos_prof_fiq_n, xdos_prof_idle_int_us;

namespace {

struct WorkerSlot {
    void (*volatile callback)(void *);
    void *volatile userData;
    volatile bool started;      // core entered Run()
    // One-shot boot job (XDOS_TUNE_PARBOOT): runs before the LVGL draw loop
    // on this core. LVGL skips a draw unit until its thread has started.
    void (*volatile bootJob)(void *);
    void *volatile bootArg;
};

WorkerSlot slots[CORES];
unsigned nextCore = 1;

// Each secondary core is the sole writer of its own idle counter.
struct alignas(64) CoreIdleSample {
    unsigned sequence;
    unsigned total;
    unsigned start;
    unsigned sleeping;
};
CoreIdleSample coreIdle[CORES]{};

void WaitForCoreWork(unsigned core)
{
    // Single writer per core: plain stores + DMB. No LDREX/STREX here: a
    // store-exclusive is itself a WFE wake-up event and would make WFE spin.
    auto &idle = coreIdle[core];
    volatile unsigned *seq = &idle.sequence;
    const unsigned s = *seq;
    *seq = s + 1;                                   // odd: update in progress
    DataMemBarrier();
    *(volatile unsigned *)&idle.start = CTimer::GetClockTicks();
    *(volatile unsigned *)&idle.sleeping = 1;
    DataMemBarrier();
    *seq = s + 2;                                   // even: consistent, sleeping
    DataSyncBarrier();
    asm volatile("wfe" ::: "memory");
    *seq = s + 3;
    DataMemBarrier();
    *(volatile unsigned *)&idle.total += CTimer::GetClockTicks() - idle.start;
    *(volatile unsigned *)&idle.sleeping = 0;
    DataMemBarrier();
    *seq = s + 4;
    DataMemBarrier();
}

class CXdosCores : public CMultiCoreSupport
{
public:
    CXdosCores() : CMultiCoreSupport(CMemorySystem::Get()) {}

    void Run(unsigned nCore) override
    {
        if (nCore == 0) return;
        slots[nCore].started = true;
        DataSyncBarrier();
        for (;;) {
            void (*job)(void *) = slots[nCore].bootJob;
            if (job) {
                DataMemBarrier();
                job(slots[nCore].bootArg);
                slots[nCore].bootJob = nullptr;
                DataSyncBarrier();
                asm volatile("sev");
                continue;
            }
            void (*callback)(void *) = slots[nCore].callback;
            if (callback) {
                DataMemBarrier();
                callback(slots[nCore].userData);  // LVGL render loop; returns on delete
                slots[nCore].callback = nullptr;
                DataSyncBarrier();
            }
            WaitForCoreWork(nCore);
        }
    }
};

CXdosCores *cores;

inline unsigned ThisCore() { return CMultiCoreSupport::ThisCore(); }

inline void Wake()
{
    DataSyncBarrier();
    asm volatile("sev");
}

// Wait for a condition: core 0 lets other Circle tasks run, cores 1..3 sleep
// in WFE until the next SEV.
// Mutex owner: the core for cores 1..3; on core 0 the current Circle task, so
// the main LVGL task and the core-0 draw task (4-core mode) are distinct.
int OwnerId()
{
    const unsigned core = ThisCore();
    if (core != 0) return (int)core;
    if (!CScheduler::IsActive()) return 0;
    return (int)(uintptr_t)CScheduler::Get()->GetCurrentTask();
}

#if LV_DRAW_SW_DRAW_UNIT_CNT >= 4
// 4-core mode: the 4th LVGL draw unit is a Circle task on core 0. It renders
// while the main LVGL task waits (Yield) and keeps LVGL alive when apps have
// borrowed all of cores 1..3. It waits on a Circle event, never with WFE, so
// it does not stall the other core-0 tasks.
class CXdosCore0DrawTask : public CTask
{
public:
    CXdosCore0DrawTask(void (*callback)(void *), void *arg)
        : CTask(TASK_STACK_SIZE), callback_(callback), arg_(arg) {}
    void Run() override { callback_(arg_); }
private:
    void (*callback_)(void *);
    void *arg_;
};
CXdosCore0DrawTask *core0DrawTask;
CSynchronizationEvent core0DrawEvent;
lv_thread_sync_t *volatile core0DrawSync;
#endif

template <typename Ready>
void WaitUntil(Ready ready)
{
    const bool main = ThisCore() == 0;
    while (!ready()) {
        if (main) {
            // Let other Circle tasks run, then sleep until a draw unit SEVs.
            if (CScheduler::IsActive()) CScheduler::Get()->Yield();
            if (ready()) break;
            const unsigned start = CTimer::GetClockTicks();
            const unsigned i0 = xdos_prof_irq_us + xdos_prof_fiq_us;
            asm volatile("wfe");
            xdos_wfi_microseconds += CTimer::GetClockTicks() - start;
            xdos_prof_idle_int_us += xdos_prof_irq_us + xdos_prof_fiq_us - i0;
        } else {
            WaitForCoreWork(ThisCore());
        }
    }
    DataMemBarrier();
}

bool StartCores()
{
    if (cores) return true;
    cores = new CXdosCores;
    if (!cores || !cores->Initialize()) {
        CLogger::Get()->Write("CORES", LogError, "secondary cores failed to start");
        return false;
    }
    CLogger::Get()->Write("CORES", LogNotice, "LVGL draw units on cores 1..%u",
                          (unsigned)LV_DRAW_SW_DRAW_UNIT_CNT);
    return true;
}

// ---- Core lending (C2 step 3-5) ------------------------------------------
// A V2 app may borrow a draw-unit core (2 or 3; core 1 always stays with LVGL
// so rendering can never stall). LVGL's dispatcher skips a lent unit
// (lv_draw_sw.c patch: xdos_draw_unit_lent), and the unit's render thread runs
// the job from lv_thread_sync_wait, i.e. only at its idle point.
struct LendState {
    volatile int lent;                     // set/cleared by core 0
    int32_t (*volatile job)(void *);       // pending job, taken by the lent core
    void *volatile arg;
    volatile int running;
    volatile int done;
    volatile int32_t result;
    volatile int stop;                     // cooperative stop request
};
LendState lend[CORES];

// Requests from cores 1..3 executed on core 0 (LVGL, FatFs, network).
struct Call0 {
    void (*volatile fn)(void *);
    void *volatile arg;
    volatile int pending;
};
Call0 call0[CORES];

#if LV_DRAW_SW_DRAW_UNIT_CNT >= 4
constexpr unsigned kFirstLendable = 1;   // core 0 draw task keeps LVGL alive
#else
constexpr unsigned kFirstLendable = 2;   // core 1 always stays with LVGL
#endif

void RunLentJob(unsigned me)
{
    LendState &l = lend[me];
    int32_t (*job)(void *) = l.job;
    if (!l.lent || !job) return;
    DataMemBarrier();
    l.running = 1;
    l.job = nullptr;
    // App code was copied on core 0: drop this core's stale I-cache/predictor.
    asm volatile("mcr p15, 0, %0, c7, c5, 0" :: "r"(0) : "memory");   // ICIALLU
    asm volatile("mcr p15, 0, %0, c7, c5, 6" :: "r"(0) : "memory");   // BPIALL
    asm volatile("dsb\n\tisb" ::: "memory");
    const int32_t r = job(l.arg);
    l.result = r;
    DataMemBarrier();
    l.running = 0;
    l.done = 1;
    Wake();
}

} // namespace

extern "C" {

int xdos_this_core(void)
{
    return (int)ThisCore();
}

int xdos_draw_unit_lent(int unit)
{
    const int core = unit + 1;   // unit 3 (4-core mode) is the core-0 task: never lent
    return core > 0 && core < (int)CORES ? lend[core].lent : 0;
}

// Lending policy (R16-B): core 0 sets how many cores may be lent at once. XDOS uses
// 3 (all of cores 1..3) when no full-screen app is running and 2 while one is, so a
// draw core stays with LVGL. Lowering the limit never takes a running job away: the
// first core returned afterwards simply is not lent again while over the limit.
static volatile unsigned s_lendLimit = CORES - 1;

unsigned xdos_core_lent_count(void)
{
    unsigned n = 0;
    for (unsigned core = 1; core < CORES; ++core) n += lend[core].lent ? 1 : 0;
    return n;
}

void xdos_core_set_lend_limit(unsigned limit)
{
    s_lendLimit = limit < CORES - 1 ? limit : CORES - 1;
}

unsigned xdos_core_lend_limit(void)
{
    return s_lendLimit;
}

// Time base for app code on any core (system timer, microseconds, wraps after ~71 min).
uint32_t xdos_core_time_us(void)
{
    return CTimer::GetClockTicks();
}

// Sleep on a lent core without the Circle scheduler (it only runs on core 0): WFE with
// this core's generic-timer event stream on (~0.43 ms wake granularity), and SEV from
// xdos_core_request_stop() ends the wait at once.
// Returns 0 after the full time, 1 when a stop was requested, -1 on core 0 (use the
// scheduler there, e.g. MsSleep).
int xdos_core_sleep_us(uint32_t us)
{
    const unsigned me = ThisCore();
    if (me == 0 || me >= CORES) return -1;
    unsigned saved;
    asm volatile("mrc p15, 0, %0, c14, c1, 0" : "=r"(saved));          // CNTKCTL
    const unsigned stream = (saved & ~0xF8U) | (12U << 4) | (1U << 2);  // EVNTI=12, EVNTEN
    asm volatile("mcr p15, 0, %0, c14, c1, 0\n\tisb" :: "r"(stream) : "memory");
    const unsigned start = CTimer::GetClockTicks();
    int r = 0;
    while ((unsigned)(CTimer::GetClockTicks() - start) < us) {
        if (lend[me].stop) { r = 1; break; }
        asm volatile("wfe");
    }
    asm volatile("mcr p15, 0, %0, c14, c1, 0\n\tisb" :: "r"(saved) : "memory");
    return r;
}

int xdos_core_lend(unsigned core, int32_t (*job)(void *), void *arg)
{
    if (ThisCore() != 0 || !job || core < kFirstLendable || core >= CORES ||
        core > (unsigned)LV_DRAW_SW_DRAW_UNIT_CNT || !cores || lend[core].lent) return 0;
    if (xdos_core_lent_count() >= s_lendLimit) return 0;   // keep a draw core (R16-B)
#if LV_DRAW_SW_DRAW_UNIT_CNT >= 4
    if (!core0DrawTask) return 0;            // never lend away the last renderer
#endif
    LendState &l = lend[core];
    l.done = 0;
    l.running = 0;
    l.stop = 0;
    l.result = 0;
    l.arg = arg;
    DataMemBarrier();
    l.lent = 1;               // dispatcher stops handing tasks to this unit
    DataMemBarrier();
    l.job = job;
    Wake();
    return 1;
}

int xdos_core_lend_any(int32_t (*job)(void *), void *arg)
{
    for (unsigned core = CORES - 1; core >= kFirstLendable; --core)
        if (xdos_core_lend(core, job, arg)) return (int)core;
    return -1;
}

int xdos_core_job_state(unsigned core, int32_t *result)
{
    if (core >= CORES) return -1;
    const LendState &l = lend[core];
    if (!l.lent) return -2;                         // not lent
    if (l.job || l.running || !l.done) return 1;    // pending or running
    DataMemBarrier();
    if (result) *result = l.result;
    return 0;                                       // finished
}

void xdos_core_request_stop(unsigned core)
{
    if (core < CORES) {
        lend[core].stop = 1;
        Wake();
    }
}

int xdos_core_stop_requested(void)
{
    return lend[ThisCore()].stop;
}

int xdos_core_reclaim(unsigned core)
{
    if (ThisCore() != 0 || core >= CORES || !lend[core].lent) return 0;
    LendState &l = lend[core];
    if (l.job || l.running || !l.done) return 0;    // still busy
    DataMemBarrier();
    l.lent = 0;
    l.done = 0;
    DataSyncBarrier();
    lv_draw_dispatch_request();                     // let LVGL use the unit again
    return 1;
}

int xdos_call_core0(void (*fn)(void *), void *arg)
{
    if (!fn) return 0;
    const unsigned me = ThisCore();
    if (me == 0) {
        fn(arg);
        return 1;
    }
    Call0 &c = call0[me];
    c.arg = arg;
    DataMemBarrier();
    c.fn = fn;
    c.pending = 1;
    Wake();
    while (c.pending) WaitForCoreWork(me);
    DataMemBarrier();
    return 1;
}

// Core 0 main loop: run requests posted by lent cores (outside LVGL refresh).
void xdos_core0_service(void)
{
    if (ThisCore() != 0) return;
    for (unsigned core = 1; core < CORES; ++core) {
        Call0 &c = call0[core];
        if (!c.pending) continue;
        DataMemBarrier();
        void (*fn)(void *) = c.fn;
        if (fn) fn(c.arg);
        c.fn = nullptr;
        DataMemBarrier();
        c.pending = 0;
        Wake();
    }
}

// Run fn(arg) once on core 1..3 before that core enters its LVGL draw loop.
// Must be called on core 0 before lv_init() hands the core to LVGL. The job
// runs with IRQs routed to core 0 and must not use the Circle scheduler.
// Returns 0 when the core is unavailable; the caller then runs fn itself.
int xdos_core_boot_job(unsigned core, void (*fn)(void *), void *arg)
{
    if (core == 0 || core >= CORES || !fn || !StartCores()) return 0;
    if (slots[core].callback || slots[core].bootJob) return 0;
    slots[core].bootArg = arg;
    DataMemBarrier();
    slots[core].bootJob = fn;
    Wake();
    return 1;
}

lv_result_t lv_thread_init(lv_thread_t *thread, const char *const, lv_thread_prio_t,
                           void (*callback)(void *), size_t, void *user_data)
{
    if (!StartCores()) return LV_RESULT_INVALID;
#if LV_DRAW_SW_DRAW_UNIT_CNT >= 4
    if (nextCore >= CORES && !core0DrawTask && CScheduler::IsActive()) {
        thread->core = 0;
        core0DrawTask = new CXdosCore0DrawTask(callback, user_data);
        return core0DrawTask ? LV_RESULT_OK : LV_RESULT_INVALID;
    }
#endif
    if (nextCore >= CORES) return LV_RESULT_INVALID;
    const unsigned core = nextCore++;
    thread->core = (int)core;
    slots[core].userData = user_data;
    DataMemBarrier();
    slots[core].callback = callback;
    Wake();
    return LV_RESULT_OK;
}

lv_result_t lv_thread_delete(lv_thread_t *)
{
    // LVGL signals exit_status first; the core returns to its WFE loop.
    return LV_RESULT_OK;
}

lv_result_t lv_mutex_init(lv_mutex_t *mutex)
{
    mutex->owner = -1;
    mutex->depth = 0;
    DataMemBarrier();
    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock(lv_mutex_t *mutex)
{
    const int me = OwnerId();
    if (mutex->owner == me) {
        ++mutex->depth;
        return LV_RESULT_OK;
    }
    WaitUntil([mutex, me] { return __sync_bool_compare_and_swap(&mutex->owner, -1, me); });
    mutex->depth = 1;
    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock_isr(lv_mutex_t *mutex)
{
    const int me = OwnerId();
    if (mutex->owner == me) {
        ++mutex->depth;
        return LV_RESULT_OK;
    }
    if (!__sync_bool_compare_and_swap(&mutex->owner, -1, me)) return LV_RESULT_INVALID;
    DataMemBarrier();
    mutex->depth = 1;
    return LV_RESULT_OK;
}

lv_result_t lv_mutex_unlock(lv_mutex_t *mutex)
{
    if (mutex->owner != OwnerId() || mutex->depth == 0) return LV_RESULT_INVALID;
    if (--mutex->depth == 0) {
        DataMemBarrier();
        mutex->owner = -1;
        Wake();
    }
    return LV_RESULT_OK;
}

lv_result_t lv_mutex_delete(lv_mutex_t *mutex)
{
    mutex->owner = -1;
    mutex->depth = 0;
    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_init(lv_thread_sync_t *sync)
{
    sync->signaled = 0;
    DataMemBarrier();
    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_wait(lv_thread_sync_t *sync)
{
    const unsigned me = ThisCore();
#if LV_DRAW_SW_DRAW_UNIT_CNT >= 4
    if (me == 0 && core0DrawTask && CScheduler::Get()->GetCurrentTask() == core0DrawTask) {
        core0DrawSync = sync;
        while (!__sync_bool_compare_and_swap(&sync->signaled, 1, 0)) {
            core0DrawEvent.Clear();
            if (__sync_bool_compare_and_swap(&sync->signaled, 1, 0)) break;
            core0DrawEvent.Wait();
        }
        DataMemBarrier();
        return LV_RESULT_OK;
    }
#endif
    if (me == 0) {
        WaitUntil([sync] { return __sync_bool_compare_and_swap(&sync->signaled, 1, 0); });
        return LV_RESULT_OK;
    }
    // Draw-unit idle point: a lent core runs its borrowed job here.
    for (;;) {
        if (__sync_bool_compare_and_swap(&sync->signaled, 1, 0)) break;
        RunLentJob(me);
        if (__sync_bool_compare_and_swap(&sync->signaled, 1, 0)) break;
        WaitForCoreWork(me);
    }
    DataMemBarrier();
    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_signal(lv_thread_sync_t *sync)
{
    DataMemBarrier();
    sync->signaled = 1;
    Wake();
#if LV_DRAW_SW_DRAW_UNIT_CNT >= 4
    // Only core 0 (the dispatcher) signals the core-0 draw unit.
    if (sync == core0DrawSync && ThisCore() == 0) core0DrawEvent.Set();
#endif
    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_signal_isr(lv_thread_sync_t *sync)
{
    return lv_thread_sync_signal(sync);
}

lv_result_t lv_thread_sync_delete(lv_thread_sync_t *sync)
{
    sync->signaled = 0;
    return LV_RESULT_OK;
}

void lv_sleep_ms(uint32_t ms)
{
    if (ThisCore() == 0 && CScheduler::IsActive()) CScheduler::Get()->MsSleep(ms);
    else CTimer::SimpleMsDelay(ms);
}

uint32_t lv_os_get_idle_percent(void)
{
    return lv_timer_get_idle();
}

uint32_t lv_os_get_proc_idle_percent(void)
{
    return lv_timer_get_idle();
}

} // extern "C"

extern "C" unsigned xdos_secondary_load(unsigned core)
{
    static unsigned lastTime[CORES]{};
    static unsigned lastIdle[CORES]{};
    if (!core || core>=CORES) return 0;
    const auto &idle=coreIdle[core];
    unsigned before, after, now, total;
    do {
        before=__atomic_load_n(&idle.sequence,__ATOMIC_ACQUIRE);
        if (before&1u) { after=before+1; continue; }
        total=__atomic_load_n(&idle.total,__ATOMIC_RELAXED);
        const bool sleeping=__atomic_load_n(&idle.sleeping,__ATOMIC_RELAXED);
        const unsigned start=__atomic_load_n(&idle.start,__ATOMIC_RELAXED);
        now=CTimer::GetClockTicks();
        if (sleeping) total+=now-start;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        after=__atomic_load_n(&idle.sequence,__ATOMIC_RELAXED);
    } while (before!=after);
    const unsigned elapsed=now-lastTime[core];
    const unsigned waiting=total-lastIdle[core];
    const bool initialized=lastTime[core]!=0;
    lastTime[core]=now;
    lastIdle[core]=total;
    if (!initialized || !elapsed || waiting>=elapsed) return 0;
    return unsigned((static_cast<uint64_t>(elapsed-waiting)*100)/elapsed);
}
