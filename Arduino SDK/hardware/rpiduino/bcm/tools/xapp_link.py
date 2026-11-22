"""Link a trusted ARM32 Arduino sketch against a paired XDOS XAPP host."""
import json
import subprocess
import tempfile
from pathlib import Path


def command(*args, capture=False):
    return subprocess.run([str(arg) for arg in args], text=True, check=True,
                          capture_output=capture).stdout


def link_xapp(output, objects, host_file):
    host = json.loads(Path(host_file).read_text(encoding="utf-8"))
    if host.get("format") != 1:
        raise ValueError("Unsupported XDOS host descriptor")
    base = int(host["slot_start"])
    end = int(host["slot_end"])
    crc = int(host["kernel_crc32"])
    if end - base != 320 * 1024 or base % 64 or not 0 <= crc <= 0xFFFFFFFF:
        raise ValueError("Invalid XDOS execution slot descriptor")
    tools = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory() as staging:
        work = Path(staging)
        entry = work / "entry.o"
        command("arm-none-eabi-g++", "-Os", "-mcpu=cortex-a53", "-marm",
                "-mfloat-abi=hard", "-mfpu=neon-vfpv4", "-ffreestanding",
                "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                "-c", tools / "xapp_entry.cpp", "-o", entry)
        combined = work / "module.o"
        command("arm-none-eabi-ld", "-r", entry, *objects, "-o", combined)
        command("arm-none-eabi-objcopy", "--remove-section=.ARM.exidx*",
                "--remove-section=.ARM.extab*", "--remove-section=.eh_frame*", combined)
        imports = {line.split()[-1] for line in command(
            "arm-none-eabi-nm", "-u", combined, capture=True).splitlines() if line.strip() and line.split()[0] == "U"}
        missing = imports.difference(host["symbols"])
        if missing:
            raise ValueError("XDOS host does not export: " + ", ".join(sorted(missing)))
        bindings = "\n".join(f"{name} = 0x{int(host['symbols'][name]):x};"
                             for name in sorted(imports))
        script = work / "module.ld"
        script.write_text(f"""OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(xapp_entry)
{bindings}
SECTIONS {{
    . = 0x{base:x};
    .header : {{
        LONG(0x50414458) LONG(1) LONG(32) LONG(image_end - 0x{base:x})
        LONG(xapp_entry - 0x{base:x}) LONG(1) LONG(0x{crc:x}) LONG(0x{base:x})
    }}
    .text : {{ *(.text*) *(.rodata*) }}
    .data : {{ *(.data*) BYTE(0) . = ALIGN(4); }}
    image_end = .;
    .bss (NOLOAD) : {{ *(.bss*) *(COMMON) }}
    ASSERT(. <= 0x{end:x}, "Sketch exceeds XDOS execution slot")
    /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.comment*) *(.note*) }}
}}
""", encoding="ascii")
        command("arm-none-eabi-ld", "--gc-sections", "-T", script, combined, "-o", output)
