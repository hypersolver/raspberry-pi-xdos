#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
for cmd in git make arm-none-eabi-gcc arm-none-eabi-g++; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd. Install GCC ARM bare-metal cross compiler packages." >&2; exit 1; }
done
if [[ ! -d circle/.git ]]; then
  git clone --depth 1 https://github.com/rsta2/circle.git circle
fi
cd circle
./configure -r 3 -f
./makeall
for sample in 08-usbkeyboard 10-usbmouse; do
  echo "=== Building ${sample} for Raspberry Pi 3B+ AArch32 ==="
  make -C "sample/${sample}"
  find "sample/${sample}" -maxdepth 1 -type f \( -name 'kernel7.img' -o -name 'kernel.img' \) -print
 done
printf '\nCircle USB keyboard and mouse test images are in circle/sample/{08-usbkeyboard,10-usbmouse}/\n'
printf 'These are STANDALONE Circle samples. They DO NOT make XD OS USB-enabled.\n'
