"""Build Arduino LVGL demos as kernel-paired, external ARM32 XAPPs."""

from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib


phase, kit, circle = sys.argv[1:]
kit, circle = Path(kit), Path(circle)
tools = Path(__file__).resolve().parent
project = tools.parents[1]
source = kit / "source/95-xdos-arm32-dualnet"
addon = circle / "addon/lvgl"
lvgl = addon / "lvgl"
work = kit / "output/arduino-demos"
apps = (("LVGLDEMO", "LVGLWidgetsDemo", "LVGL Widgets"),
        ("LVGLBENCH", "LVGLBenchmarkDemo", "LVGL Benchmark"))


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, text=True, **kwargs)


def imports(obj):
    lines = run("arm-none-eabi-nm", "-u", obj, capture_output=True).stdout.splitlines()
    return {line.split()[-1] for line in lines if line.strip() and
            not line.split()[-1].startswith("__aeabi_unwind_cpp_")}


def set_flags(text, names):
    for name in names:
        text, count = re.subn(rf"(?m)^(\s*#define {name}[ \t]+)[01]\b",
                              lambda match: match[1] + "1", text)
        if count != 1:
            raise RuntimeError(f"Cannot set {name}")
    return text


def prepare():
    work.mkdir(parents=True, exist_ok=True)
    config = addon / "lv_conf.h"
    text = config.read_text(encoding="utf-8")
    text = set_flags(text, ("LV_USE_SYSMON", "LV_USE_PERF_MONITOR"))
    if text != config.read_text(encoding="utf-8"):
        config.write_text(text, encoding="utf-8")
    (work / "lv_conf.h").write_text(set_flags(text, ("LV_USE_DEMO_WIDGETS",
        "LV_USE_DEMO_BENCHMARK", "LV_FONT_MONTSERRAT_26")), encoding="utf-8")

    bench = lvgl / "demos/benchmark/lv_demo_benchmark.c"
    text = bench.read_text(encoding="utf-8")
    if "void xdos_stop_official_benchmark" not in text:
        edits = {
            "static uint32_t scene_act;": "static lv_timer_t * xdos_scene_timer;\n"
                "static lv_obj_t * xdos_title;\nstatic uint32_t scene_act;",
            "lv_obj_t * title = lv_label_create(lv_layer_top());":
                "lv_obj_t * title = xdos_title = lv_label_create(lv_layer_top());",
            "lv_timer_create(next_scene_timer_cb, scenes[0].scene_time, NULL);":
                "xdos_scene_timer = lv_timer_create(next_scene_timer_cb, scenes[0].scene_time, NULL);",
            "        lv_timer_delete(timer);": "        lv_timer_delete(timer);\n        xdos_scene_timer = NULL;",
            "void lv_demo_benchmark_set_end_cb(": """void xdos_stop_official_benchmark(void)
{
    if(xdos_scene_timer) lv_timer_delete(xdos_scene_timer);
    xdos_scene_timer = NULL;
    if(xdos_title) lv_obj_delete(xdos_title);
    xdos_title = NULL;
    lv_anim_delete(lv_layer_top(), color_anim_cb);
    lv_obj_set_style_bg_opa(lv_layer_top(), LV_OPA_TRANSP, 0);
}

void lv_demo_benchmark_set_end_cb("""
        }
        for before, after in edits.items():
            if text.count(before) != 1:
                raise RuntimeError(f"Unexpected Benchmark layout: {before}")
            text = text.replace(before, after)
        bench.write_text(text, encoding="utf-8")

    flags = ["-Os", "-mcpu=cortex-a53", "-marm", "-mfloat-abi=hard", "-mfpu=neon-vfpv4",
             "-ffreestanding", "-ffunction-sections", "-fdata-sections", "-fno-unwind-tables",
             "-fno-asynchronous-unwind-tables", "-DRASPPI=3", "-DAARCH=32",
             "-DLV_CONF_INCLUDE_SIMPLE", "-I", work, "-I", lvgl, "-I", circle / "include",
             "-I", source, "-I", source / "xdos_arduino", "-I", tools]

    def compile_c(path):
        obj = work / (path.stem + ".o")
        run("arm-none-eabi-gcc", *flags, "-c", path, "-o", obj)
        return obj

    widgets = [compile_c(p) for p in sorted((lvgl / "demos/widgets").rglob("*.c"))]
    benchmark = [compile_c(p) for p in sorted((lvgl / "demos/benchmark").rglob("*.c"))]
    font26 = compile_c(lvgl / "src/font/lv_font_montserrat_26.c")
    all_imports = set()
    for app, sketch, _ in apps:
        sketch_obj = work / f"{app}-sketch.o"
        entry_obj = work / f"{app}-entry.o"
        run("arm-none-eabi-g++", *flags, "-x", "c++", "-std=c++17", "-DRPIDUINO_LVGL_HOST_TIMER",
            "-c", project / f"arduino-demos/{sketch}/{sketch}.ino", "-o", sketch_obj)
        options = ["-DXDOS_BENCHMARK_SKETCH"] if app == "LVGLBENCH" else []
        run("arm-none-eabi-g++", *flags, "-std=c++17", *options,
            "-c", tools / "sketch_xapp.cpp", "-o", entry_obj)
        combined = work / f"{app}.rel.o"
        objects = widgets + (benchmark + [font26] if options else [])
        inputs = [sketch_obj, entry_obj, *objects]
        (work / f"{app}.inputs").write_text(
            "\n".join(path.name for path in inputs) + "\n", encoding="ascii")
        run("arm-none-eabi-ld", "-r", *inputs, "-o", combined)
        run("arm-none-eabi-objcopy", "--remove-section=.ARM.exidx*",
            "--remove-section=.ARM.extab*", "--remove-section=.eh_frame*", combined)
        all_imports.update(imports(combined))
    (source / "xapp-imports.mk").write_text(
        "LDFLAGS += " + " ".join(f"-u {symbol}" for symbol in sorted(all_imports)) + "\n",
        encoding="ascii")
    print(f"Arduino LVGL XAPP objects ready; retaining {len(all_imports)} shared imports")


def finish():
    kernel = circle / "sample/103-xdos-pi3-usb-msc/kernel8-32.elf"
    raw = (kit / "output/xdos32-v062-rc3-pi3.img").read_bytes()
    symbols = {}
    for line in run("arm-none-eabi-nm", "-g", "--defined-only", kernel,
                    capture_output=True).stdout.splitlines():
        address, _, name = line.split()
        symbols[name] = int(address, 16)
    start, stop, slot, slot_end = (symbols[s] for s in
        ("_start", "_etext", "xdos_xapp_exec_slot", "xdos_xapp_exec_slot_end"))
    assert start < stop <= start + len(raw) <= slot < slot_end
    fingerprint = zlib.crc32(raw[:stop-start])
    for app, _, title in apps:
        obj = work / f"{app}.rel.o"
        missing = imports(obj) - symbols.keys()
        if missing:
            raise RuntimeError(f"{app}: kernel is missing imports {sorted(missing)}")
        bindings = "\n".join(f"{name} = 0x{symbols[name]:x};" for name in sorted(imports(obj)))
        script = work / f"{app}.ld"
        script.write_text(f"""OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(xapp_entry)
{bindings}
SECTIONS {{
    . = 0x{slot:x};
    .header : {{ LONG(0x50414458) LONG(1) LONG(32)
        LONG(image_end - 0x{slot:x}) LONG(xapp_entry - 0x{slot:x})
        LONG(1) LONG(0x{fingerprint:x}) LONG(0x{slot:x}) }}
    .text : {{ *(.text*) *(.rodata*) }}
    .data : {{ *(.data*) . = ALIGN(4); }}
    image_end = .;
    .bss (NOLOAD) : {{ *(.bss*) *(COMMON) }}
    ASSERT(. <= 0x{slot_end:x}, "XAPP exceeds execution slot with BSS")
    /DISCARD/ : {{ *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) *(.comment*) *(.note*) }}
}}
""", encoding="ascii")
        elf = work / f"{app}.elf"
        inputs = [work / name for name in (work / f"{app}.inputs").read_text().splitlines()]
        run("arm-none-eabi-ld", "--gc-sections", "-T", script, *inputs, "-o", elf)
        run("arm-none-eabi-objcopy", "--strip-symbol=__aeabi_unwind_cpp_pr0",
            "--strip-symbol=__aeabi_unwind_cpp_pr1", "--strip-symbol=__aeabi_unwind_cpp_pr2", elf)
        image = kit / "output" / f"{app}.XAPP"
        run("arm-none-eabi-objcopy", "-O", "binary", elf, image)
        data = image.read_bytes()
        magic, abi, head, length, entry, flags, crc, base = struct.unpack("<4s7I", data[:32])
        assert (magic,abi,head,length,flags,crc,base) == (b"XDAP",1,32,len(data),1,fingerprint,slot)
        assert 32 <= entry < len(data) and not entry % 4
        assert not run("arm-none-eabi-nm", "-u", elf, capture_output=True).stdout.strip()
        (kit / "output" / f"{app}.APP").write_text(
            f"[Application]\nName={title}\nExec=SD:/XDOS/APPS/{app}.XAPP\nABI=1\nArch=armv7-a32\n",
            encoding="ascii")
        print(f"{app}.XAPP: {len(data)} bytes, bound to kernel CRC32 {fingerprint:08x}")


{"prepare": prepare, "finish": finish}[phase]()
