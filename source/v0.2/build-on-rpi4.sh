#!/usr/bin/env bash
# Build on RPi4 Ubuntu AArch64, target Pi3B AArch32.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CIRCLE="${1:-$HOME/xdos-build/circle}"
if [[ ! -f "$CIRCLE/Config.mk" ]]; then
  echo "ERROR: Circle checkout not found: $CIRCLE" >&2; exit 1
fi
if ! grep -Eq '^RASPPI[[:space:]]*=[[:space:]]*3([[:space:]]|$)' "$CIRCLE/Config.mk" || \
   ! grep -Eq '^AARCH[[:space:]]*=[[:space:]]*32([[:space:]]|$)' "$CIRCLE/Config.mk"; then
  echo 'ERROR: Circle needs RASPPI = 3 and AARCH = 32.' >&2; exit 1
fi
if [[ ! -f "$CIRCLE/lib/usb/libusb.a" ]]; then
  echo 'ERROR: First run cd ~/xdos-build/circle && ./makeall' >&2; exit 1
fi
DST="$CIRCLE/sample/91-xdos-arm32-asm"
mkdir -p "$DST"
cp "$ROOT"/source/91-xdos-arm32-asm/* "$DST/"
# Clean build guarantees that older C++ prototype object files cannot leak in.
make -C "$DST" clean
make -C "$DST" -j2
IMAGE="$DST/kernel8-32.img"
if [[ ! -s "$IMAGE" ]]; then
  echo "ERROR: expected compiled image was not produced: $IMAGE" >&2; exit 1
fi
mkdir -p "$ROOT/output"
cp "$IMAGE" "$ROOT/output/xdos-arm32-asm-v02.img"
sha256sum "$ROOT/output/xdos-arm32-asm-v02.img" > "$ROOT/output/SHA256SUM.txt"
echo 'Build complete:'
ls -lh "$ROOT/output/xdos-arm32-asm-v02.img"
