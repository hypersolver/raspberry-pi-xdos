# XD OS ARM32 — Raspberry Pi 3B+ direct-boot V0.1 prototype

**Status:** Compiled ARM32 bare-metal framebuffer demo. **Not tested on physical Raspberry Pi 3B+ hardware.** It is **not** yet a port of the original x86 XD OS kernel; it is a new proof-of-concept static GUI that demonstrates the intended boot path and gives a clean base for later migration.

## What it does

- Custom ARMv7/AArch32 bare-metal startup at ARM physical `0x8000`.
- Parks secondary cores and clears the `.bss` section.
- Talks to Raspberry Pi 3B+ VideoCore through mailbox property channel 8.
- Requests a 1024x768x32 framebuffer and draws a demo desktop.
- Does not run Linux or QEMU.

**Not implemented:** USB mouse/keyboard, timer/interrupt handling, filesystem, Ethernet/Wi-Fi, browser, original x86 code or applications. Demo windows and icons are decorative, not interactive.

## Hardware

- Raspberry Pi 3 Model B+ (BCM2837B0)
- A spare microSD card (do **not** overwrite the card you currently use)
- HDMI monitor, 5V supply

## Prepare microSD card

1. Prepare a FAT32 boot partition on the spare SD card. You can use a Raspberry Pi OS image solely to obtain a known-good Pi 3B+ boot partition, without using its Linux kernel/root filesystem.
2. **Keep the Raspberry Pi firmware files** from a working Pi 3B+ boot partition. The standard set includes `bootcode.bin`, `start.elf`, and `fixup.dat`. Keep its board Device Tree and overlay files if already present. If you need the firmware files, see the official Raspberry Pi firmware repository: https://github.com/raspberrypi/firmware/tree/master/boot
3. Copy the included `xdos32.img` to the FAT boot partition root.
4. Back up the existing `config.txt` then replace it with the included `config.txt`. The config explicitly requests ARM32 and 1024x768 HDMI.
5. Power off the Pi 3B+. Insert this test card, attach HDMI, then power on.

**Expected test result** (if firmware and framebuffer path work): an XD OS ARM32 static desktop showing `HDMI FRAMEBUFFER OK`, plus decorative windows. If nothing is shown, the boot / framebuffer path needs debugging on real hardware.

## Compile again

Cross-compile on a computer with LLVM Clang + LLD + llvm-objcopy:

```sh
make
```

Outputs:

- `xdos32.elf` debug/linkable image, entry at `0x8000`
- `xdos32.img` raw image for the Pi firmware

This Makefile uses `clang`, `ld.lld`, and `llvm-objcopy`. It has been compiled using clang 17 on the authoring machine. This is a first validation of the toolchain, not validation on a Raspberry Pi 3B+.

## Port roadmap

1. V0.1 — ARM32 startup and framebuffer (this package, untested on hardware).
2. V0.2 — Timer, interrupt controller, mouse/keyboard input.
3. V0.3 — Move/redraw windows and lightweight UI widgets.
4. V0.4 — Port core apps, text/paint/calculator.
5. V0.5 — SD/FAT32, networking, and consider TLS security/certificate verification.

Original XD OS repository: https://github.com/eklavya-raj/xdos (MIT License). This prototype is independent and does not incorporate any original repository code. Do not confuse this demo with the completed OS port.
