"""Remove onboard Wi-Fi while keeping the microSD and Ethernet paths."""

from pathlib import Path
import sys


source = Path(sys.argv[1])


def edit(name, old, new):
    path = source / name
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"{name}: missing or ambiguous Ethernet-only anchor {old!r}")
    path.write_text(text.replace(old, new), encoding="utf-8")


edit("kernel.h", "#if XDOS_HAS_WIFI\n#include <SDCard/emmc.h>\n#include <fatfs/ff.h>\n",
     "#include <SDCard/emmc.h>\n#include <fatfs/ff.h>\n#if XDOS_HAS_WIFI\n")
edit("kernel.h", "#if XDOS_HAS_WIFI\n    CEMMCDevice emmc_;\n    FATFS filesystem_;\n",
     "    CEMMCDevice emmc_;\n    FATFS filesystem_;\n#if XDOS_HAS_WIFI\n")
edit("kernel.cpp", "#if XDOS_HAS_WIFI\n  emmc_(&interrupt_,&timer_,&actLED_),\n",
     "  emmc_(&interrupt_,&timer_,&actLED_),\n#if XDOS_HAS_WIFI\n")
edit("kernel.cpp", "#if XDOS_HAS_WIFI\n    // FAT32 is needed", "    // FAT32 is needed")
edit("kernel.cpp", '#endif\n    desktop_.SetNetwork(false,"ETH: DETECTING...");',
     '    desktop_.SetNetwork(false,"ETH: DETECTING...");')
edit("Makefile", "        $(CIRCLEHOME)/addon/wlan/libwlan.a " + chr(92) +
     "\n        $(CIRCLEHOME)/addon/fatfs/libfatfs.a " + chr(92) +
     "\n        $(CIRCLEHOME)/addon/SDCard/libsdcard.a $(LIBS)\nendif",
     "        $(CIRCLEHOME)/addon/wlan/libwlan.a $(LIBS)\nendif\n"
     "LIBS := $(CIRCLEHOME)/addon/fatfs/libfatfs.a $(CIRCLEHOME)/addon/SDCard/libsdcard.a $(LIBS)")
# The RC3 kit passes XDOS_WIFI=1 to make even for a wired-only build.
edit("Makefile", "XDOS_WIFI ?= $(if $(filter 3 4,$(RASPPI)),1,0)", "override XDOS_WIFI := 0")
