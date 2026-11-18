# XD OS ARM32 Assembly-level Port V0.2 (Pi 3B)

- **Build host:** RPi 4 / Ubuntu 24.04.5 ARM64
- **Target:** Raspberry Pi **3B** (not 3B+) / Cortex-A53 / ARM32 bare metal
- **Circle:** `RASPPI = 3`, `AARCH = 32`, `PREFIX = arm-none-eabi-`
- **Output:** `output/xdos-arm32-asm-v02.img` (created on your RPi 4)

This package is the **assembly-first variant** of the interactive V0.2 desktop prototype. It ports the original XD OS x86 GUI routines (`clip`/`rect`, glyph raster loop, window hit-test, dragging) to actual AArch32 Assembly (`xdos_gui.S`). It integrates with Circle's already-verified Pi 3B USB keyboard/mouse and framebuffer hardware backend. See `ASM_PORT_MAP.md` for precise origin/function mapping and known differences.

**It is not a complete 19 KB x86-to-ARM translation.** The C++ event/application shell remains; the original BIOS boot code, network driver, V86 games and most original apps have not yet been ported. No Linux or QEMU runs on the target. The USB keyboard/mouse have been tested *separately* by the user, but not yet together with this new integrated desktop.

## A — Windows: transfer this ZIP to your RPi 4

Download `XDOS_ARM32_ASM_V0.2_RPi3B.zip` in Windows, then open a **Windows PowerShell**, not the SSH terminal:

```powershell
scp ".\XDOS_ARM32_ASM_V0.2_RPi3B.zip" BUILD_USER@BUILD_HOST:/path/to/xdos-build/
ssh BUILD_USER@BUILD_HOST
```

Run the PowerShell commands from the folder containing the ZIP. To list matching files, use `Get-ChildItem ".\*XDOS*"`.

## B — RPi 4: compile and link ARM32 bare-metal image

In the RPi 4 SSH terminal:

```bash
cd /path/to/xdos-build
unzip -o XDOS_ARM32_ASM_V0.2_RPi3B.zip
cd XDOS_ARM32_ASM_V0.2_RPi3B
bash ./build-on-rpi4.sh
ls -lh output/xdos-arm32-asm-v02.img
```

The script creates an **independent** Circle sample `circle/sample/91-xdos-arm32-asm/`, so the previously verified `08-usbkeyboard`, `10-usbmouse`, and original V0.1 kernel are left untouched. It invokes Circle's own ARM32 Makefile, compiling `xdos_gui.S` with the ARM cross-toolchain.

A successful build creates `kernel8-32.img` internally, copied to `output/xdos-arm32-asm-v02.img`.

If build fails, run:

```bash
cd /path/to/xdos-build/circle
make -C sample/91-xdos-arm32-asm -j1
```

Copy the first compiler/linker error for diagnosis.

## C — Windows: copy the image back, then onto MicroSD

Use **Windows PowerShell**:

```powershell
scp BUILD_USER@BUILD_HOST:/path/to/xdos-build/XDOS_ARM32_ASM_V0.2_RPi3B/output/xdos-arm32-asm-v02.img ".\xdos-arm32-asm-v02.img"
```

Insert the Pi 3B MicroSD into Windows. Assuming its FAT32 boot partition is `E:` (change to the actual letter):

```powershell
$BootVolume = '<MicroSD boot volume>'
Copy-Item (Join-Path $BootVolume 'config.txt') ".\config-before-asm-v02.txt"
Copy-Item ".\xdos-arm32-asm-v02.img" (Join-Path $BootVolume 'xdos-arm32-asm-v02.img')
notepad (Join-Path $BootVolume 'config.txt')
```

Keep the **known-good Circle config** that worked for your USB tests, and set the only effective kernel line to:

```ini
arm_64bit=0
kernel=xdos-arm32-asm-v02.img
```

Do not remove `bootcode.bin`, `start.elf`, `fixup.dat`, or the original `xdos32.img`. Safely eject the card.

## D — Test on Pi 3B

1. Power on with keyboard, mouse, and HDMI connected. A windowed desktop with Terminal and Notepad should appear.
2. The USB status should show `KEY OK` and `MOUSE OK` when *both* devices enumerate successfully.
3. Drag the Terminal title bar, close/reopen the window, enter `help` + Enter.
4. Open Calculator and Paint by clicking desktop/taskbar icons; Paint draws small sample dots.
5. If it does not boot, restore the backed-up `config.txt` and select the known-good Circle USB test image.

**Verification status:** Assembly syntax/ELF target checked offline; Pi 4 Circle cross-link and actual Pi 3B end-to-end test remain pending.

## Licensing

Original XD OS: MIT. Circle: GPL-3.0-or-later. This ZIP contains an assembly translation and original integration shell only; it does not include a copy of the Circle library or the entire original XD OS tree. Any linked redistribution must honor Circle's license terms.
