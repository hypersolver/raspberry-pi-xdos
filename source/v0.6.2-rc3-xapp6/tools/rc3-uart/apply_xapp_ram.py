"""Reserve a dedicated, executable AArch32 section outside the boot image."""

from pathlib import Path
import sys

source, kit, circle = map(Path, sys.argv[1:])


def edit(path, before, after):
    text = path.read_text(encoding="utf-8")
    if after in text:
        return
    if text.count(before) != 1:
        raise RuntimeError(f"Unexpected source layout in {path}: {before!r}")
    path.write_text(text.replace(before, after), encoding="utf-8")


slot = source / "xapp_exec_slot.S"
edit(slot, '.section .text.xdos_xapp_slot, "ax", %progbits',
     '.section .xdos_xapp_slot, "awx", %nobits')
edit(slot, """ * This slot is linked into .text and included in the RPi3 executable mapping.""",
     """ * This slot is reserved in a dedicated NOLOAD section and mapped at boot.""")

edit(circle / "circle.ld", "\t_end = .;", """\tASSERT(. <= 0x300000, "kernel sections overlap XDOS XAPP RAM")
\t.xdos_xapp 0x300000 (NOLOAD) : {
\t\t_xdos_xapp_ram_start = .;
\t\tKEEP(*(.xdos_xapp_slot))
\t\t. = ALIGN(0x100000);
\t\t_xdos_xapp_ram_end = .;
\t}
\tASSERT(SIZEOF(.xdos_xapp) == 0x100000, "invalid XDOS RAM reservation")
\t_end = .;""")

edit(circle / "lib/pagetable.cpp",
     "\tfor (unsigned nEntry = 0; nEntry < 4096; nEntry++)",
     """\textern u8 _xdos_xapp_ram_start, _xdos_xapp_ram_end;
\tconst u32 xappStart = (u32) &_xdos_xapp_ram_start;
\tconst u32 xappEnd = (u32) &_xdos_xapp_ram_end;
\tassert((xappStart & (MEGABYTE-1)) == 0 &&
\t       xappEnd-xappStart == MEGABYTE && xappEnd <= MEM_KERNEL_END &&
\t       xappEnd <= nMemSize);

\tfor (unsigned nEntry = 0; nEntry < 4096; nEntry++)""")
edit(circle / "lib/pagetable.cpp", "\t\tif (nBaseAddress < (u32) &_etext)",
     """\t\tif (nBaseAddress < (u32) &_etext ||
\t\t    (nBaseAddress >= xappStart && nBaseAddress < xappEnd))""")

loader = source / "xapp_loader.cpp"
edit(loader, "then copy into an EXECUTABLE .text slot.", "then copy into the executable RAM slot.")
edit(loader, """ * Slot is defined by xapp_exec_slot.S and guaranteed to precede _etext
 * by tools/check-exec-section.py after full Circle link.""",
     """ * The dedicated slot is reserved at boot and excluded from the disk image.""")
edit(loader, """    const auto slot = reinterpret_cast<uintptr_t>(xdos_xapp_exec_slot);
    const auto after = reinterpret_cast<uintptr_t>(xdos_xapp_exec_slot_end);
    const auto end = reinterpret_cast<uintptr_t>(_etext);
    for (auto address = first; address < end; ++address) {
        if (address == slot) address = after;
        if (address == end) break;""", """    const auto end = reinterpret_cast<uintptr_t>(_etext);
    for (auto address = first; address < end; ++address) {""")
edit(loader, "     * This is NOT an XN workaround: .text itself is executable. */",
     "     * The reserved RAM section has executable permissions from startup. */")

checker = kit / "tools/check-exec-section.py"
checker.write_text('''#!/usr/bin/env python3
"""Verify Pi 3 XAPP RAM has no file-backed payload."""
import re
import subprocess
import sys

elf = sys.argv[1]
nm = subprocess.check_output(["arm-none-eabi-nm", "-g", "--defined-only", elf], text=True)
symbols = {line.split()[2]: int(line.split()[0],16) for line in nm.splitlines()}
start = symbols["_xdos_xapp_ram_start"]
end = symbols["_xdos_xapp_ram_end"]
slot = symbols["xdos_xapp_exec_slot"]
slot_end = symbols["xdos_xapp_exec_slot_end"]
assert start == slot == 0x300000 and end == 0x400000
assert slot_end-slot == 320*1024 and symbols["__bss_end"] <= start
assert symbols["_etext"] <= symbols["__bss_end"] and symbols["_end"] == end
sections = subprocess.check_output(["arm-none-eabi-readelf", "-SW", elf], text=True)
assert re.search(r"\\.xdos_xapp\\s+NOBITS\\s+0*300000\\s+[0-9a-f]+\\s+0*100000\\b", sections)
print(f"XAPP RAM: 0x{slot:X}..0x{slot_end:X}, dedicated 1 MiB mapping, NOLOAD")
''', encoding="utf-8")
