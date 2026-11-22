#ifndef XDOS_XAPP_LOADER_V2_H
#define XDOS_XAPP_LOADER_V2_H
/* XAPP V2 loader: four relocatable execution slots (C3).
 * Call only from core 0 at TASK_LEVEL (FatFs, cache maintenance).
 * V1 modules keep using XappLoaderV1 and its fixed slot at 0x300000. */
#include "xdos_sdk_v1.h"
#include "xdos_xapp_v2.h"
#include <stdint.h>

class XappLoaderV2 {
public:
    enum Result {
        OK = 0, NOT_READY = -1, OPEN_FAILED = -2, SIZE_INVALID = -3,
        READ_FAILED = -4, FORMAT_INVALID = -5, MEMORY_FAILED = -6,
        ENTRY_ERROR = -7, NO_SLOT = -8, KERNEL_MISMATCH = -9,
        RELOC_INVALID = -10
    };
    static constexpr unsigned Slots = XDOS_XAPP_V2_SLOTS;

    /* Slot ownership. A slot stays held while its module's code can still be
     * called (LVGL callbacks, timers); release only after the clean-up order
     * in COLLAB C2 (objects deleted, draw units idle). */
    static int AcquireSlot(bool large = false);
    static unsigned SlotBytes(unsigned slot);                  /* lowest free slot, or -1 */
    static bool HoldSlot(unsigned slot);
    static void ReleaseSlot(unsigned slot);    /* zeroes slot RAM + cache sync */
    static bool IsHeld(unsigned slot);
    static uintptr_t SlotBase(unsigned slot);  /* 0 when slot is invalid */
    static bool LayoutOk();                    /* linker reserved 3 x 320 KiB plus 768 KiB at 0x400000 */

    /* 1 (V1), 2 (V2) or 0 (not an XAPP / unreadable). */
    static int AbiOfFile(const char *path);

    /* Load `path` into `slot` (must be held by the caller) and call its entry
     * on this core. requireBound: refuse modules not bound to this kernel. */
    static Result ExecuteFile(const char *path, const XdosHostApiV1 *api,
                              unsigned slot, bool requireBound = true);

    /* Loader reason for the last FORMAT/KERNEL/RELOC failure (English). */
    static const char *LastError();
};
#endif
