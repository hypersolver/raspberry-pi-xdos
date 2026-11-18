#!/usr/bin/env bash
# RPi4 Ubuntu ARM64 -> Pi 3B/3B+ ARM32 bare metal, one dual-network image
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CIRCLE="${CIRCLE_HOME:-$HOME/xdos-build/circle}"
DEST="$CIRCLE/sample/95-xdos-arm32-dualnet"
if [[ ! -f "$CIRCLE/Config.mk" ]]; then
    echo "ERROR: Circle not found at $CIRCLE" >&2; exit 1
fi
if ! grep -Eq '^RASPPI[[:space:]]*=[[:space:]]*3([[:space:]]|$)' "$CIRCLE/Config.mk" ||
   ! grep -Eq '^AARCH[[:space:]]*=[[:space:]]*32([[:space:]]|$)' "$CIRCLE/Config.mk"; then
    echo "ERROR: Set RASPPI = 3, AARCH = 32 in Circle Config.mk" >&2; exit 1
fi
for app in arm-none-eabi-g++ arm-none-eabi-objdump git make python3; do
    command -v "$app" >/dev/null || { echo "Missing command: $app" >&2; exit 1; }
done
mkdir -p "$DEST" "$ROOT/output"
cp "$ROOT/source/95-xdos-arm32-dualnet/"* "$DEST/"
# Patch TCP connect timeout, idempotently, with backup. This modifies Circle.
python3 "$ROOT/tools/patch-circle-tcp-connect.py" "$CIRCLE"
make -C "$CIRCLE/lib/net" clean
make -C "$CIRCLE/lib/net" -j2
# WLAN has an external WPA supplicant submodule.
git -C "$CIRCLE" submodule update --init addon/wlan/hostap
make -C "$CIRCLE/addon/SDCard" -j2
make -C "$CIRCLE/addon/fatfs" -j2
(cd "$CIRCLE/addon/wlan" && ./makeall --nosample)
make -C "$DEST" clean
make -C "$DEST" -j2
IMG="$DEST/kernel8-32.img"
test -s "$IMG" || { echo 'ERROR: Circle image not generated' >&2; exit 1; }
cp "$IMG" "$ROOT/output/xdos32-eth-wifi.img"
(cd "$ROOT/output" && sha256sum xdos32-eth-wifi.img > SHA256SUM.txt)
echo "READY: $ROOT/output/xdos32-eth-wifi.img"
echo 'Copy this .img to Windows using scp and then onto the microSD FAT32 partition.'
