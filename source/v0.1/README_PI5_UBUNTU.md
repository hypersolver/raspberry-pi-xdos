# Raspberry Pi 4 (Ubuntu ARM64) build host for XD OS ARM32 (Pi 3B+)

This directory contains the previously tested-on-user-hardware V0.1 ARM32 XD OS-inspired **static framebuffer demo**, plus ARM64-Ubuntu-host build scripts. It does **not** include a functional XD OS USB stack, nor the original XD OS x86 GUI applications.

## Roles
- Pi 4 Ubuntu ARM64: **build host**.
- Pi 3B+: **AArch32 bare-metal target** (boots the generated `xdos32.img`).
- Running Ubuntu on the Pi 4 does not imply Ubuntu or QEMU on the Pi 3B+.

## Install prerequisites (on Pi 4 Ubuntu)

```bash
sudo apt update
sudo apt install -y git make build-essential clang lld llvm file unzip \
    gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi
uname -m  # expected: aarch64
```

## Rebuild V0.1 (HDMI desktop without USB)

```bash
cd /path/to/xdos-arm32-build
./build-v01-on-pi5.sh
```

The output `xdos32.img` is intended for the **Pi 3B+**, with the boot firmware that you already proved working on your SD card. Copy only the new `.img` to the **Pi 3B+ boot partition**; keep the existing firmware and `config.txt` which selects `kernel=xdos32.img`. Make a backup first.

The Makefile uses `clang --target=armv7a-none-eabi -mcpu=cortex-a53` and `llvm-objcopy`. Because this is a bare-metal build, it does not link Ubuntu's ARM64 system libraries.

## Build real Pi 3B+ USB hardware tests separately (Circle)

```bash
cd /path/to/xdos-arm32-build
./build-circle-usb-tests-on-pi5.sh
```

This downloads and builds Circle for **RASPPI=3/AArch32** and the standalone examples:

- `circle/sample/08-usbkeyboard/kernel7.img`: keyboard hardware test.
- `circle/sample/10-usbmouse/kernel7.img`: mouse hardware test.

Circle official docs: https://circle-rpi.readthedocs.io/en/50.0/getting-started.html
Sample list: https://github.com/rsta2/circle/blob/master/sample/README

**Important: these are different bare-metal programs, not USB drivers already merged into XD OS.** Their purpose is to validate the Pi 3B+ DWC USB host, onboard hub, HID devices and deployment pipeline before USB integration work.

### Safe installation of a USB test image onto the Pi 3B+ SD card

1. Shut down both boards, remove the **Pi 3B+** SD card and attach it to the Pi 4 with a USB SD reader.
2. Identify its mounted FAT32 boot volume using `lsblk -f`. **Do not modify the Pi 4 Ubuntu boot partition.**
3. Back up the boot volume's `config.txt` and `xdos32.img`.
4. Copy the built Circle keyboard test `kernel7.img` to the Pi 3B+ boot volume under the name `usbtest.img`.
5. For this **standalone Circle test**, use Circle's `boot/config32.txt` as `config.txt` and add `kernel=usbtest.img` as its last line. Keep the existing Pi 3B+ firmware on the SD card.
6. `sync`, safely unmount the SD card, then boot the Pi 3B+ with keyboard attached.
7. To return to your known-good desktop, restore your prior `config.txt`, which selects `kernel=xdos32.img`.

Example after **you** have confirmed the real boot volume mount point:

```bash
BOOT=/path/to/pi3b-boot-volume  # REPLACE with the Pi 3B+ FAT boot volume verified by lsblk -f
cp "$BOOT/config.txt" "$BOOT/config.txt.pre-usbtest"
cp circle/sample/08-usbkeyboard/kernel7.img "$BOOT/usbtest.img"
cp circle/boot/config32.txt "$BOOT/config.txt"
printf '\nkernel=usbtest.img\n' >> "$BOOT/config.txt"
sync
```

A Circle sample may require serial console output depending on the sample's configuration; consult the individual sample README/source when evaluating results. On a real USB host, test a simple wired USB keyboard and mouse first.

## Licensing

Circle is GPL-3.0. If you integrate its source code into XD OS and redistribute binaries or code, review the license's obligations. The script only clones Circle; no Circle source is bundled in this package. The earlier independent XD OS-inspired desktop demo is not the original x86 XD OS kernel.

## State

- Original V0.1 ARM32 static display: user confirmed boots and displays HDMI on physical Pi 3B+.
- The V0.1 source rebuilt in an x86_64 Linux environment using Clang and LLVM.
- Build host: Raspberry Pi 4 running Ubuntu ARM64. Circle USB test hardware outcomes require separate verification.

Build host correction: Raspberry Pi 4 Ubuntu ARM64. Filenames containing pi5 are retained from the original package for compatibility.
