/* XD OS XAPP V2 loader core: validation, relocation, BSS (see xdos_xapp_v2.h).
 * Portable C99; built into the kernel and into the host-side test library. */
#include "xdos_xapp_v2.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void wr32(uint8_t *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

uint32_t xdos_xapp_hash(const char *name)
{
    uint32_t h = 2166136261u;
    for (; name && *name; ++name) {
        h ^= (uint8_t)*name;
        h *= 16777619u;
    }
    return h;
}

/* Kernel export table (generated, see xdos_xapp_v2.h). Weak: the first kernel link
 * and the host tests run without one; then only bound modules load. */
extern const XdosExport xdos_exports[] __attribute__((weak));
extern const uint32_t xdos_export_count __attribute__((weak));

static int find_export(uint32_t hash, uint32_t *addr)
{
    if (!xdos_exports || !&xdos_export_count) return 0;
    uint32_t lo = 0, hi = xdos_export_count;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (xdos_exports[mid].hash < hash) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= xdos_export_count || xdos_exports[lo].hash != hash) return 0;
    *addr = xdos_exports[lo].addr;
    return 1;
}

/* Name of import i for an error message (names follow the hash array). */
static const char *import_name(const uint8_t *names, uint32_t names_bytes, uint32_t index)
{
    uint32_t at = 0;
    for (uint32_t i = 0; i < index && at < names_bytes; ++i)
        while (at < names_bytes && names[at++]) {}
    for (uint32_t end = at; end < names_bytes; ++end)    /* must be NUL-terminated in the file */
        if (!names[end]) return (const char *)names + at;
    return "?";
}

int xdos_xapp_abi_of(const uint8_t *file, uint32_t file_bytes)
{
    if (!file || file_bytes < 12 || memcmp(file, "XDAP", 4) != 0) return 0;
    const uint32_t abi = rd32(file + 4);
    return abi == 1u || abi == 2u ? (int)abi : 0;
}

static int fail(uint8_t *dest, uint32_t dest_bytes, const char **error,
                const char *why, int code)
{
    if (dest) memset(dest, 0, dest_bytes);
    if (error) *error = why;
    return code;
}

int xdos_xapp_v2_prepare(const uint8_t *file, uint32_t file_bytes,
                         uint8_t *dest, uint32_t dest_bytes, uint32_t runtime_base,
                         uint32_t kernel_fingerprint, uint32_t *entry,
                         const char **error)
{
    if (error) *error = "ok";
    if (!file || !dest || !entry || file_bytes < XDOS_XAPP_V2_HEADER_BYTES)
        return fail(dest, dest_bytes, error, "file too small", XDOS_XAPP_V2_E_HEADER);

    XdosXappHeaderV2 h;
    memcpy(&h, file, sizeof h);
    if (memcmp(h.magic, "XDAP", 4) != 0 || h.abi_version != XDOS_XAPP_V2_ABI ||
        h.header_bytes != XDOS_XAPP_V2_HEADER_BYTES || h.image_bytes != file_bytes)
        return fail(dest, dest_bytes, error, "bad V2 header", XDOS_XAPP_V2_E_HEADER);
    const int imports = (h.flags & XDOS_XAPP_V2_FLAG_IMPORTS) != 0;
    if (imports ? h.flags != XDOS_XAPP_V2_FLAG_IMPORTS || h.checksum > XDOS_XAPP_V2_MAX_IMPORTS
                : (h.flags & ~XDOS_XAPP_V2_FLAG_BOUND) != 0 ||
                  (!(h.flags & XDOS_XAPP_V2_FLAG_BOUND) && h.checksum != 0))
        return fail(dest, dest_bytes, error, "bad V2 flags", XDOS_XAPP_V2_E_HEADER);
    const uint32_t import_count = imports ? h.checksum : 0;
    const uint32_t tables = h.reloc_count * (uint32_t)sizeof(XdosXappRelocV2) + import_count * 4u;
    if (h.load_bytes < XDOS_XAPP_V2_HEADER_BYTES || h.load_bytes > h.reloc_offset ||
        (h.reloc_offset & 3u) || h.reloc_count > XDOS_XAPP_V2_MAX_RELOCS ||
        h.reloc_offset > file_bytes ||
        (imports ? file_bytes - h.reloc_offset < tables : file_bytes - h.reloc_offset != tables))
        return fail(dest, dest_bytes, error, "bad V2 layout", XDOS_XAPP_V2_E_HEADER);
    const uint8_t *hashes = file + h.reloc_offset + h.reloc_count * (uint32_t)sizeof(XdosXappRelocV2);
    const uint8_t *names = hashes + import_count * 4u;
    const uint32_t names_bytes = imports ? file_bytes - h.reloc_offset - tables : 0;
    if (h.entry_offset < XDOS_XAPP_V2_HEADER_BYTES || h.entry_offset >= h.load_bytes ||
        (h.entry_offset & 3u))
        return fail(dest, dest_bytes, error, "bad V2 entry", XDOS_XAPP_V2_E_HEADER);
    if ((h.link_base & 63u) || (runtime_base & 63u))
        return fail(dest, dest_bytes, error, "unaligned base", XDOS_XAPP_V2_E_HEADER);
    if (h.bss_bytes > dest_bytes || h.load_bytes > dest_bytes - h.bss_bytes)
        return fail(dest, dest_bytes, error, "module does not fit slot", XDOS_XAPP_V2_E_SLOT);
    if ((h.flags & XDOS_XAPP_V2_FLAG_BOUND) && h.checksum != kernel_fingerprint)
        return fail(dest, dest_bytes, error, "module bound to another kernel", XDOS_XAPP_V2_E_KERNEL);
    /* Every import must be exported, used or not: __xdos_abi_<n> gates the ABI. */
    for (uint32_t i = 0; i < import_count; ++i) {
        uint32_t addr;
        if (!find_export(rd32(hashes + 4u * i), &addr)) {
            static char why[96];
            const char *name = import_name(names, names_bytes, i);
            strcpy(why, "kernel lacks import ");
            strncat(why, name, sizeof why - strlen(why) - 1);
            return fail(dest, dest_bytes, error, why, XDOS_XAPP_V2_E_IMPORT);
        }
    }

    /* Module address range at link time: [link_base, link_end]. `link_end`
     * itself is allowed (end-of-object pointers, e.g. image_end). */
    const uint32_t mem_bytes = h.load_bytes + h.bss_bytes;
    const uint32_t link_base = h.link_base;
    const uint32_t link_end = link_base + mem_bytes;
    if (link_end < link_base || runtime_base + mem_bytes < runtime_base)
        return fail(dest, dest_bytes, error, "address overflow", XDOS_XAPP_V2_E_HEADER);
    const uint32_t delta = runtime_base - link_base;   /* modulo 2^32 */

    memset(dest, 0, dest_bytes);                       /* BSS and tail are zero */
    memcpy(dest, file, h.load_bytes);

    const uint8_t *rel = file + h.reloc_offset;
    for (uint32_t i = 0; i < h.reloc_count; ++i, rel += sizeof(XdosXappRelocV2)) {
        const uint32_t off = rd32(rel);
        const uint32_t type = rd32(rel + 4);
        if ((off & 3u) || off < XDOS_XAPP_V2_HEADER_BYTES || off > h.load_bytes - 4u)
            return fail(dest, dest_bytes, error, "relocation outside load image", XDOS_XAPP_V2_E_RELOC);
        uint8_t *site = dest + off;
        const uint32_t word = rd32(site);
        if (type == XDOS_RELOC_ABS32_INTERNAL) {
            if (word < link_base || word > link_end)
                return fail(dest, dest_bytes, error, "ABS32 target outside module", XDOS_XAPP_V2_E_RANGE);
            wr32(site, word + delta);
        } else if (type == XDOS_RELOC_BRANCH_EXTERNAL) {
            /* ARM B/BL: cond != 1111, bits 27..25 = 101. BLX(imm): 1111 101H. */
            if (((word >> 25) & 7u) != 5u)
                return fail(dest, dest_bytes, error, "branch relocation on non-branch", XDOS_XAPP_V2_E_RELOC);
            const int blx = (word >> 28) == 0xFu;
            int32_t imm = (int32_t)(word << 8) >> 6;           /* sign-extended imm24 * 4 */
            if (blx) imm += (int32_t)((word >> 23) & 2u);       /* H bit */
            const uint32_t target = link_base + off + 8u + (uint32_t)imm;
            if (target >= link_base && target < link_end)
                return fail(dest, dest_bytes, error, "external branch targets module", XDOS_XAPP_V2_E_RELOC);
            const int64_t disp = (int64_t)target - (int64_t)(runtime_base + off + 8u);
            if (disp < -(int64_t)0x2000000 || disp > (int64_t)0x1FFFFFC ||
                (!blx && (disp & 3)) || (blx && (disp & 1)))
                return fail(dest, dest_bytes, error, "branch out of range", XDOS_XAPP_V2_E_RANGE);
            uint32_t patched = (word & 0xFF000000u) | (((uint32_t)(disp >> 2)) & 0x00FFFFFFu);
            if (blx) patched = (patched & ~(1u << 24)) | ((uint32_t)(disp & 2) << 23);
            wr32(site, patched);
        } else if (type == XDOS_RELOC_IMPORT_ABS32 && imports) {
            const uint32_t rel_word = word - XDOS_XAPP_V2_IMPORT_BASE;
            const uint32_t index = rel_word >> XDOS_XAPP_V2_IMPORT_SHIFT;
            uint32_t addr = 0;
            if (word < XDOS_XAPP_V2_IMPORT_BASE || index >= import_count ||
                !find_export(rd32(hashes + 4u * index), &addr))
                return fail(dest, dest_bytes, error, "bad import relocation", XDOS_XAPP_V2_E_RELOC);
            wr32(site, addr + (rel_word & ((1u << XDOS_XAPP_V2_IMPORT_SHIFT) - 1u)));
        } else {
            return fail(dest, dest_bytes, error, "unknown relocation type", XDOS_XAPP_V2_E_RELOC);
        }
    }

    *entry = runtime_base + h.entry_offset;
    return XDOS_XAPP_V2_OK;
}
