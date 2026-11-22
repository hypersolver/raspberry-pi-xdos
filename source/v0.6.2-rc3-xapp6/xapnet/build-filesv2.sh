#!/usr/bin/env bash
set -euo pipefail
base=$1
out=$2
src=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d "${base%/*}/filesv2.XXXXXX")
mkdir -p "$out"
echo "$work" > "$out/workdir.txt"
cp -a "$base/circle" "$work/circle"
circle=$work/circle
sample=$circle/sample/xdos-sdboot
cp "$src"/kernel/* "$sample/"
tool=$(realpath "$src/../../../../CLAUD XDOS/tools/mc4/xappv2")
flags=(-Os -mcpu=cortex-a53 -marm -mfloat-abi=hard -mfpu=neon-vfpv4 -ffreestanding -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti -mword-relocations -DRASPPI=3 -DAARCH=32 -DLV_CONF_INCLUDE_SIMPLE -I "$circle/addon/lvgl/lvgl" -I "$circle/addon/lvgl" -I "$circle/include" -I "$sample" -Wall -Wextra)
for app in files notepad; do
    arm-none-eabi-g++ "${flags[@]}" -c "$src/apps/$app.cpp" -o "$work/$app.o"
done
arm-none-eabi-nm -u "$work/files.o" "$work/notepad.o" | awk '$1=="U" {print "LDFLAGS += --undefined=" $2}' | sort -u >> "$sample/xapp-imports.mk"
make -C "$sample" -j4
cp "$sample/kernel8-32.img" "$out/FVWIND.IMG"
cp "$sample/kernel8-32.elf" "$out/kernel8-32.elf"
PYTHONPATH="$tool" "${PACK_PYTHON:-python3}" - "$work" "$out" <<'PY'
from pathlib import Path
import sys
import xapp_v2_pack as packer
work,out=map(Path,sys.argv[1:])
symbols=packer.kernel_symbols(out/'kernel8-32.elf')
fp=packer.kernel_fingerprint(symbols,out/'FVWIND.IMG')
for source,target in [('files','FWFILES.XAPP'),('notepad','FWNOTE.XAPP')]:
    print(target, packer.pack([work/(source+'.o')],symbols,fp,out/target))
PY
(cd "$out" && sha256sum FVWIND.IMG FWFILES.XAPP FWNOTE.XAPP > SHA256SUMS.txt)
