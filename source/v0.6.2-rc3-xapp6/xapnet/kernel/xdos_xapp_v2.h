/* XD OS XAPP V2: relocatable ARM32 module format (C3).
 *
 * A V2 module is linked once at XDOS_XAPP_V2_LINK_BASE (V2 slot 0) and can be
 * loaded into any of the four V2 execution slots. The loader copies the load
 * image, applies the relocation table and zeroes BSS. V1 modules (fixed to the
 * V1 slot at 0x300000) are unchanged and keep using XappLoaderV1.
 *
 * File layout (little endian):
 *   [0, 48)                     XdosXappHeaderV2
 *   [48, load_bytes)            .text/.rodata/.data, linked at link_base
 *   [reloc_offset, image_bytes) reloc_count x XdosXappRelocV2
 * Memory layout in a slot: load image, then bss_bytes of zero.
 *
 * Kernel-independent modules (R17, flags & XDOS_XAPP_V2_FLAG_IMPORTS): the module
 * does not contain kernel addresses. Calls to kernel functions go through a PLT
 * inside the module (ldr pc,[pc,#-4]; .word import), kernel data is reached by
 * XDOS_RELOC_IMPORT_ABS32 words. After the relocation table follow
 *   checksum x uint32_t   FNV-1a hashes of the imported kernel symbol names
 *   names                 NUL-separated names (only for error messages)
 * and the loader binds each hash through the kernel's export table. Such a module
 * loads on any kernel that exports all its imports, including __xdos_abi_<n>.
 *
 * Plain C, no Circle headers: shared by the kernel and the host-side tests.
 */
#ifndef XDOS_XAPP_V2_H
#define XDOS_XAPP_V2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XDOS_XAPP_V2_ABI          2u
#define XDOS_XAPP_V2_HEADER_BYTES 48u
#define XDOS_XAPP_V2_SLOTS        4u
#define XDOS_XAPP_V2_SLOT_BYTES   (320u * 1024u)
#define XDOS_XAPP_V2_LARGE_SLOT_BYTES (768u * 1024u)
#define XDOS_XAPP_V2_SLOT0        0x400000u            /* slot i = SLOT0 + i*SLOT_BYTES */
#define XDOS_XAPP_V2_LINK_BASE    XDOS_XAPP_V2_SLOT0
#define XDOS_XAPP_V2_FLAG_BOUND   1u                   /* checksum = kernel fingerprint */
#define XDOS_XAPP_V2_FLAG_IMPORTS 2u                   /* checksum = import count (R17) */
#define XDOS_XAPP_V2_IMPORT_BASE  0x80000000u          /* IMPORT_ABS32 word = BASE + (i << 16) + addend */
#define XDOS_XAPP_V2_IMPORT_SHIFT 16u
#define XDOS_XAPP_V2_MAX_IMPORTS  8192u
#define XDOS_XAPP_V2_MAX_RELOCS   65536u

typedef struct XdosXappHeaderV2 {
    uint8_t  magic[4];        /* X D A P (same magic as V1) */
    uint32_t abi_version;     /* 2 */
    uint32_t header_bytes;    /* 48 */
    uint32_t image_bytes;     /* whole file, including the relocation table */
    uint32_t entry_offset;    /* from slot start, ARM state, 4-aligned */
    uint32_t flags;           /* XDOS_XAPP_V2_FLAG_BOUND or 0 */
    uint32_t checksum;        /* kernel fingerprint when bound, else 0 */
    uint32_t link_base;       /* address the image was linked at */
    uint32_t load_bytes;      /* bytes copied into the slot (header included) */
    uint32_t bss_bytes;       /* zero-filled after the load image */
    uint32_t reloc_offset;    /* file offset of the relocation table */
    uint32_t reloc_count;
} XdosXappHeaderV2;

/* Relocation kinds. Everything else is resolved at pack time. */
enum {
    XDOS_RELOC_ABS32_INTERNAL = 1,  /* 32-bit word holding a module address: += delta */
    XDOS_RELOC_BRANCH_EXTERNAL = 2, /* ARM B/BL/BLX(imm) to a fixed kernel address */
    XDOS_RELOC_IMPORT_ABS32 = 3     /* 32-bit word: kernel import + addend, bound at load (R17) */
};

/* Kernel export table (R17): generated after the first kernel link, sorted by hash.
 * addr keeps bit 0 for Thumb functions. The kernel links it as xdos_exports. */
typedef struct XdosExport {
    uint32_t hash;            /* xdos_xapp_hash(name) */
    uint32_t addr;
} XdosExport;

/* FNV-1a, 32 bit, of a symbol name. */
uint32_t xdos_xapp_hash(const char *name);

typedef struct XdosXappRelocV2 {
    uint32_t offset;          /* from slot start, 4-aligned, inside the load image */
    uint32_t type;
} XdosXappRelocV2;

enum {
    XDOS_XAPP_V2_OK = 0,
    XDOS_XAPP_V2_E_HEADER = -1,     /* magic/abi/sizes/entry */
    XDOS_XAPP_V2_E_SLOT = -2,       /* image + bss does not fit the slot */
    XDOS_XAPP_V2_E_KERNEL = -3,     /* bound to another kernel build */
    XDOS_XAPP_V2_E_RELOC = -4,      /* malformed relocation entry */
    XDOS_XAPP_V2_E_RANGE = -5,      /* relocation target outside the module / branch range */
    XDOS_XAPP_V2_E_IMPORT = -6      /* kernel does not export an imported symbol (R17) */
};

/* Validate `file` and materialise it in `dest` (dest_bytes bytes of slot RAM)
 * as if dest were mapped at `runtime_base`. On success *entry receives the
 * absolute entry address. On failure dest is zeroed and *error (if not NULL)
 * receives a short English reason. Caller performs cache maintenance. */
int xdos_xapp_v2_prepare(const uint8_t *file, uint32_t file_bytes,
                         uint8_t *dest, uint32_t dest_bytes, uint32_t runtime_base,
                         uint32_t kernel_fingerprint, uint32_t *entry,
                         const char **error);

/* Cheap header sniff: 1 for V1, 2 for V2, 0 for anything else. */
int xdos_xapp_abi_of(const uint8_t *file, uint32_t file_bytes);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
static_assert(sizeof(XdosXappHeaderV2) == XDOS_XAPP_V2_HEADER_BYTES, "XAPP V2 header ABI");
static_assert(sizeof(XdosXappRelocV2) == 8, "XAPP V2 reloc ABI");
#else
_Static_assert(sizeof(XdosXappHeaderV2) == XDOS_XAPP_V2_HEADER_BYTES, "XAPP V2 header ABI");
_Static_assert(sizeof(XdosXappRelocV2) == 8, "XAPP V2 reloc ABI");
#endif

#endif
