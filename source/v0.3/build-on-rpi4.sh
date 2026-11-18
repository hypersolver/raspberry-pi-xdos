#!/usr/bin/env bash
# Host: Raspberry Pi 4 Ubuntu 24.04 ARM64; target: Raspberry Pi 3B ARM32 bare metal.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CIRCLE="${1:-$HOME/xdos-build/circle}"
DEST="$CIRCLE/sample/92-xdos-arm32-net"
if [[ ! -f "$CIRCLE/Config.mk" ]]; then
  echo "ERROR: Circle Config.mk not found at $CIRCLE" >&2; exit 1
fi
if ! grep -Eq '^RASPPI[[:space:]]*=[[:space:]]*3([[:space:]]|$)' "$CIRCLE/Config.mk" || \
   ! grep -Eq '^AARCH[[:space:]]*=[[:space:]]*32([[:space:]]|$)' "$CIRCLE/Config.mk"; then
  echo "ERROR: Circle must be configured for RASPPI=3, AARCH=32" >&2; exit 1
fi
for lib in lib/usb/libusb.a lib/input/libinput.a lib/net/libnet.a lib/sched/libsched.a lib/libcircle.a; do
 if [[ ! -f "$CIRCLE/$lib" ]]; then
  echo "ERROR: Missing $CIRCLE/$lib, run: cd '$CIRCLE' && ./makeall" >&2; exit 1
 fi
done
command -v arm-none-eabi-g++ >/dev/null || { echo 'Missing arm-none-eabi-g++' >&2;exit 1; }
mkdir -p "$DEST" "$ROOT/output"
cp "$ROOT/source/92-xdos-arm32-net/"* "$DEST/"
# Prevent stale V0.2 objects from influencing the result.
make -C "$DEST" clean
make -C "$DEST" -j2
IMAGE="$DEST/kernel8-32.img"
test -s "$IMAGE" || { echo 'ERROR: kernel8-32.img missing' >&2;exit 1; }
cp "$IMAGE" "$ROOT/output/xdos32-v03.img"
(cd "$ROOT/output" && sha256sum xdos32-v03.img > SHA256SUM.txt)
echo
printf 'SUCCESS: %s\n' "$ROOT/output/xdos32-v03.img"
ls -lh "$ROOT/output/xdos32-v03.img"
echo 'From Windows PowerShell:'
