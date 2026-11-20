"""Build four native USB modules against the matching Circle kernel."""

from pathlib import Path
import struct
import subprocess
import sys
import zlib


phase, kit_arg, circle_arg = sys.argv[1:]
kit, circle = Path(kit_arg), Path(circle_arg)
source = kit/"source/95-xdos-arm32-dualnet"
tools = Path(__file__).parent
out = kit/"output"
work = out/"usb-drivers-build"
work.mkdir(parents=True, exist_ok=True)
archive = circle/"lib/usb/libusb.a"
modules = {
    "KEYBOARD": ("usbkeyboard.o", "usbkeyboard8bitdo.o"),
    "MOUSE": ("usbmouse.o",),
    "STORAGE": ("usbmassdevice.o",),
    "TOUCH": ("usbtouchscreen.o",),
}


def run(*args):
    return subprocess.check_output([str(arg) for arg in args])


def imports(obj):
    return {line.split()[-1] for line in run("arm-none-eabi-nm","-u",obj).decode().splitlines()}


def prepare():
    shared = set()
    for index,(name,objects) in enumerate(modules.items()):
        entry = work/f"{name}-entry.o"
        run("arm-none-eabi-g++","-Os","-std=c++17","-marm","-mcpu=cortex-a53",
            "-mfloat-abi=hard","-mfpu=neon-fp-armv8","-ffreestanding","-fno-exceptions",
            "-fno-rtti","-fno-unwind-tables","-fno-asynchronous-unwind-tables",
            "-ffunction-sections","-fdata-sections","-DAARCH=32","-DRASPPI=3",
            f"-DXDOS_DRIVER={index}","-I",circle/"include","-c",tools/"driver_entry.cpp","-o",entry)
        driver_objects = []
        for member in objects:
            path = work/member
            path.write_bytes(run("arm-none-eabi-ar","p",archive,member))
            driver_objects.append(path)
        combined = work/f"{name}.o"
        run("arm-none-eabi-ld","-r",entry,*driver_objects,"-o",combined)
        shared.update(imports(combined))
    with (source/"xapp-imports.mk").open("a",encoding="ascii") as file:
        file.write("LDFLAGS += " + " ".join(f"-u {symbol}" for symbol in sorted(shared)) + "\n")


def finish():
    kernel = circle/"sample/103-xdos-pi3-usb-msc/kernel8-32.elf"
    raw = (out/"xdos32-v062-rc3-pi3.img").read_bytes()
    symbols = {}
    for line in run("arm-none-eabi-nm","-g","--defined-only",kernel).decode().splitlines():
        address,_,name = line.split()
        symbols[name] = int(address,16)
    fingerprint = zlib.crc32(raw[:symbols["_etext"]-symbols["_start"]])
    for index,name in enumerate(modules):
        address = 0x350000 + index * 0x20000
        assert address >= symbols["xdos_xapp_exec_slot_end"] and address+0x20000 <= 0x400000
        obj = work/f"{name}.o"
        missing = imports(obj)-symbols.keys()
        if missing:
            raise RuntimeError(f"{name}: missing kernel imports {sorted(missing)}")
        bindings = "\n".join(f"{symbol} = 0x{symbols[symbol]:x};" for symbol in sorted(imports(obj)))
        keyboard = ("LONG(xdos_driver_keyboard_handler - slot) LONG(xdos_driver_keyboard_leds - slot)"
                    if index == 0 else "LONG(0) LONG(0)")
        keyboard_exports = ("EXTERN(xdos_driver_keyboard_handler xdos_driver_keyboard_leds)"
                            if index == 0 else "")
        script = work/f"{name}.ld"
        script.write_text(f"""OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(xdos_driver_create)
{keyboard_exports}
{bindings}
SECTIONS {{
    slot = 0x{address:x};
    . = slot;
    .header : {{
        LONG(0x31524458) LONG({index}) LONG(file_end - slot) LONG(0x{fingerprint:08x})
        LONG(slot) LONG(xdos_driver_create - slot) {keyboard}
        LONG(init_start - slot) LONG(init_end - slot)
    }}
    .text : {{ *(.text*) *(.rodata*) }}
    . = ALIGN(4);
    .init_array : {{
        init_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array))
        init_end = .;
    }}
    .data : {{ *(.data*) BYTE(0) . = ALIGN(4); }}
    file_end = .;
    .bss (NOLOAD) : {{ *(.bss*) *(COMMON) }}
    .ctors : {{ KEEP(*(.ctors*)) }}
    ASSERT(SIZEOF(.ctors) == 0, "Unexpected legacy constructor table")
    ASSERT(. <= slot + 0x20000, "USB driver exceeds its slot")
    /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.comment*) *(.note*) }}
}}
""",encoding="ascii")
        elf, binary = work/f"{name}.elf", out/f"{name}.DRV"
        run("arm-none-eabi-ld","--gc-sections","-T",script,obj,"-o",elf)
        run("arm-none-eabi-objcopy","-O","binary",elf,binary)
        data = binary.read_bytes()
        fields = struct.unpack("<10I",data[:40])
        assert fields[:5] == (0x31524458,index,len(data),fingerprint,address) and 40 <= fields[5] < len(data)
        if index == 0:
            assert all(40 <= offset < len(data) and offset % 4 == 0 for offset in fields[6:8])
        assert not run("arm-none-eabi-nm","-u",elf).strip()
        print(f"{binary.name}: {len(data)} bytes, kernel CRC32 {fingerprint:08x}")


{"prepare": prepare, "finish": finish}[phase]()
