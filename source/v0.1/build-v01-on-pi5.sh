#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
for cmd in clang ld.lld llvm-objcopy make file; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd. Install with sudo apt install clang lld llvm make file" >&2; exit 1; }
done
echo "Build host architecture: $(uname -m)"
make clean
make
file xdos32.elf
file xdos32.img
sha256sum xdos32.img
printf '\nBuilt: %s/xdos32.img\n' "$PWD"
printf 'Target: Raspberry Pi 3B+ AArch32 bare-metal, NOT Raspberry Pi 5 native boot.\n'
printf 'V0.1 does NOT include USB support.\n'
