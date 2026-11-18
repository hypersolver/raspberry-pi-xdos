#!/usr/bin/env bash
set -euo pipefail
CIRCLE="${CIRCLE_HOME:-$HOME/xdos-build/circle}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Downloads vendor firmware from locations maintained by Circle.
make -C "$CIRCLE/addon/wlan/firmware"
mkdir -p "$ROOT/output/firmware"
# Firmware filenames for Pi 3B (43430) and 3B+ (43455).
for f in brcmfmac43430-sdio.bin brcmfmac43430-sdio.txt brcmfmac43430-sdio.clm_blob \
         brcmfmac43455-sdio.bin brcmfmac43455-sdio.txt brcmfmac43455-sdio.clm_blob; do
 test -s "$CIRCLE/addon/wlan/firmware/$f" || { echo "Missing firmware $f" >&2;exit 1; }
 cp "$CIRCLE/addon/wlan/firmware/$f" "$ROOT/output/firmware/$f"
done
(cd "$ROOT/output" && zip -q -r -FS wifi-firmware.zip firmware)
echo "Firmware package: $ROOT/output/wifi-firmware.zip"
