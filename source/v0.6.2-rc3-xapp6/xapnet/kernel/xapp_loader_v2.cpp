#include "xapp_loader_v2.h"
#include <fatfs/ff.h>
#include <circle/synchronize.h>
#include <string.h>

/* Four V2 execution slots, NOLOAD, inside the executable XDOS XAPP RAM
 * (0x300000..0x600000, see circle.ld / pagetable.cpp patches). */
extern "C" unsigned char xdos_xapp_v2_slots[];
extern "C" unsigned char xdos_xapp_v2_slots_end[];
extern "C" const unsigned char _start[], _etext[];

namespace {

bool held[XDOS_XAPP_V2_SLOTS];
const char *lastError = "ok";

/* Same fingerprint as V1 (CRC32 of kernel text); V2 slots lie above _etext. */
uint32_t KernelFingerprint()
{
    static bool computed;
    static uint32_t fingerprint;
    if (computed) return fingerprint;
    uint32_t crc = ~0U;
    for (const unsigned char *p = _start; p < _etext; ++p) {
        crc ^= *p;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0U);
    }
    fingerprint = ~crc;
    computed = true;
    return fingerprint;
}

void Trace(const XdosHostApiV1 *api, const char *s)
{
    if (api && api->log) api->log(api->context, s);
}

unsigned char *SlotPtr(unsigned slot)
{
    return xdos_xapp_v2_slots + slot * XDOS_XAPP_V2_SLOT_BYTES;
}

/* Code is about to run on cores 1..3 too (borrowed cores): invalidate the
 * instruction caches and branch predictors of all cores (inner shareable). */
void SyncAllCores()
{
    SyncDataAndInstructionCache();
    asm volatile("mcr p15, 0, %0, c7, c1, 0" :: "r"(0) : "memory");   /* ICIALLUIS */
    asm volatile("mcr p15, 0, %0, c7, c1, 6" :: "r"(0) : "memory");   /* BPIALLIS */
    asm volatile("dsb ish\n\tisb" ::: "memory");
}

} // namespace

bool XappLoaderV2::LayoutOk()
{
    return reinterpret_cast<uintptr_t>(xdos_xapp_v2_slots) == XDOS_XAPP_V2_SLOT0 &&
           static_cast<uintptr_t>(xdos_xapp_v2_slots_end - xdos_xapp_v2_slots) ==
               (Slots-1) * XDOS_XAPP_V2_SLOT_BYTES + XDOS_XAPP_V2_LARGE_SLOT_BYTES;
}

uintptr_t XappLoaderV2::SlotBase(unsigned slot)
{
    return slot < Slots ? reinterpret_cast<uintptr_t>(SlotPtr(slot)) : 0;
}

unsigned XappLoaderV2::SlotBytes(unsigned slot)
{
    return slot>=Slots ? 0 : slot==Slots-1 ? XDOS_XAPP_V2_LARGE_SLOT_BYTES : XDOS_XAPP_V2_SLOT_BYTES;
}

int XappLoaderV2::AcquireSlot(bool large)
{
    for (unsigned i = large ? Slots-1 : 0; i < Slots; ++i)
        if (HoldSlot(i)) return (int)i;
    return -1;
}

bool XappLoaderV2::HoldSlot(unsigned slot)
{
    if (slot >= Slots || held[slot]) return false;
    held[slot] = true;
    return true;
}

void XappLoaderV2::ReleaseSlot(unsigned slot)
{
    if (slot >= Slots || !held[slot]) return;
    memset(SlotPtr(slot), 0, SlotBytes(slot));
    SyncAllCores();
    held[slot] = false;
}

bool XappLoaderV2::IsHeld(unsigned slot)
{
    return slot < Slots && held[slot];
}

const char *XappLoaderV2::LastError()
{
    return lastError;
}

int XappLoaderV2::AbiOfFile(const char *path)
{
    FIL file;
    if (!path || f_open(&file, path, FA_READ) != FR_OK) return 0;
    uint8_t head[12];
    UINT got = 0;
    const FRESULT fr = f_read(&file, head, sizeof head, &got);
    f_close(&file);
    return fr == FR_OK ? xdos_xapp_abi_of(head, got) : 0;
}

XappLoaderV2::Result XappLoaderV2::ExecuteFile(const char *path, const XdosHostApiV1 *api,
                                               unsigned slot, bool requireBound)
{
    lastError = "ok";
    if (!LayoutOk()) { lastError = "V2 slot layout"; return MEMORY_FAILED; }
    if (!IsHeld(slot)) return NO_SLOT;
    if (!path || !api || api->abi_version != XDOS_ABI_VERSION ||
        api->struct_bytes < sizeof(XdosHostApiV1)) return FORMAT_INVALID;

    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) return OPEN_FAILED;
    const FSIZE_t fileSize = f_size(&file);
    /* Load image plus relocation table; the table may add up to 512 KiB. */
    if (fileSize < XDOS_XAPP_V2_HEADER_BYTES ||
        fileSize > SlotBytes(slot) + XDOS_XAPP_V2_MAX_RELOCS * 8u) {
        f_close(&file);
        return SIZE_INVALID;
    }
    unsigned char *image = new unsigned char[(unsigned)fileSize];
    if (!image) { f_close(&file); return MEMORY_FAILED; }
    UINT got = 0;
    const FRESULT fr = f_read(&file, image, (UINT)fileSize, &got);
    const FRESULT closeResult = f_close(&file);
    if (fr != FR_OK || closeResult != FR_OK || got != (UINT)fileSize) {
        delete [] image;
        return READ_FAILED;
    }
    XdosXappHeaderV2 h;
    memcpy(&h, image, sizeof h);
    if (requireBound && !(h.flags & XDOS_XAPP_V2_FLAG_BOUND)) {
        delete [] image;
        lastError = "module not bound to a kernel";
        return FORMAT_INVALID;
    }

    uint32_t entryAddress = 0;
    const char *why = 0;
    const int rc = xdos_xapp_v2_prepare(image, (uint32_t)fileSize, SlotPtr(slot),
                                        SlotBytes(slot), (uint32_t)SlotBase(slot),
                                        KernelFingerprint(), &entryAddress, &why);
    delete [] image;
    if (rc != XDOS_XAPP_V2_OK) {
        lastError = why ? why : "prepare failed";
        SyncDataAndInstructionCache();
        Trace(api, "[XAPP2] Module rejected: ");
        Trace(api, lastError);
        Trace(api, "\r\n");
        return rc == XDOS_XAPP_V2_E_KERNEL ? KERNEL_MISMATCH :
               rc == XDOS_XAPP_V2_E_SLOT   ? SIZE_INVALID :
               rc == XDOS_XAPP_V2_E_HEADER ? FORMAT_INVALID : RELOC_INVALID;
    }
    /* Copied + relocated instructions must reach the instruction fetch path
     * of every core (jobs may run on a borrowed core). */
    SyncAllCores();
    Trace(api, "[XAPP2] Module relocated; entering ARM32 module\r\n");
    const XdosAppEntryV1 entry = reinterpret_cast<XdosAppEntryV1>(entryAddress);
    const int32_t appResult = entry(api);
    Trace(api, "[XAPP2] ARM32 module returned to XD OS\r\n");
    return appResult == 0 ? OK : ENTRY_ERROR;
}
