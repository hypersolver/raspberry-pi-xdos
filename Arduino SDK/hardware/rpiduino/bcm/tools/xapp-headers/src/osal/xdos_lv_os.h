/* LV_OS_CUSTOM types for XDOS on Circle (bare metal, Raspberry Pi 3).
 *
 * LVGL software draw units run as "threads" pinned to cores 1..3. Core 0
 * keeps the Circle scheduler, USB, network, UART and VC4 presentation.
 * Copied next to lv_os_private.h so both the kernel and XAPP builds find it.
 */
#ifndef XDOS_LV_OS_H
#define XDOS_LV_OS_H

typedef struct {
    int core;                   /* core running the thread, 1..3 */
} lv_thread_t;

typedef struct {
    volatile int owner;         /* core holding the lock, -1 when free */
    volatile unsigned depth;    /* recursion depth (LVGL expects recursive) */
} lv_mutex_t;

typedef struct {
    volatile int signaled;      /* set by signal, consumed by wait */
} lv_thread_sync_t;

#endif
