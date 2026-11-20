"""Configure the Pi test build for 921600 baud and the boot splash hold."""

from pathlib import Path
import sys


source = Path(sys.argv[1])


def replace(name, pairs):
    path = source / name
    text = path.read_text()
    for old, new in pairs:
        if text.count(old) != 1:
            raise RuntimeError(f"{name}: expected one occurrence of {old!r}")
        text = text.replace(old, new)
    path.write_text(text, encoding="utf-8")


replace("kernel.cpp", (
    ("serial_.Initialize(115200)", "serial_.Initialize(921600)"),
    ("115200 8N1 enabled", "921600 8N1 enabled"),
    ("XAPP6-PERF5 / Pi 3 ARM32", "XAPP6-PERF7 / Pi 3 ARM32"),
    ('    boot_.Finish("BOOT READY - opening XD OS desktop");',
     '    boot_.Finish("BOOT READY - opening XD OS desktop");\n    scheduler_.MsSleep(150);'),
))
replace("boot_splash.cpp", (("115200 baud", "921600 baud"),))
replace("app_shortcut.cpp", (
    ("P5WIDGET.XAPP", "P7WIDGET.XAPP"),
    ("P5BENCH.XAPP", "P7BENCH.XAPP"),
))
