#!/usr/bin/env bash
# Build the R16-B test XAPPs against a kernel built by ../../build-filesv2.sh.
# usage: build.sh <circle tree used for the kernel> <kernel out dir with <tag>.IMG + kernel8-32.elf> <tag> <prefix>
# Produces <prefix>CORE.XAPP (Core Demo, forever), <prefix>SLOW.XAPP (15 ms core-0 slices),
# <prefix>SLEEP.XAPP (xdos_core_sleep_us test).
set -euo pipefail
circle=$1; out=$2; tag=$3; prefix=$4
here=$(cd "$(dirname "$0")" && pwd)
sample=$circle/sample/xdos-sdboot
tool="$here/../../xappv2-tools"
work=$(mktemp -d)
flags=(-Os -mcpu=cortex-a53 -marm -mfloat-abi=hard -mfpu=neon-vfpv4 -ffreestanding -ffunction-sections
       -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -mword-relocations
       -DRASPPI=3 -DAARCH=32 -DLV_CONF_INCLUDE_SIMPLE -I "$circle/addon/lvgl/lvgl" -I "$circle/addon/lvgl"
       -I "$circle/include" -I "$sample" -I "$here/../../kernel" -Wall -Wextra)
arm-none-eabi-gcc "${flags[@]}" -DXDOS_DEMO_FOREVER -c "$here/coredemo.c" -o "$work/core.o"
arm-none-eabi-gcc "${flags[@]}" -DXDOS_DEMO_FOREVER -DXDOS_DEMO_NO_LEND -DR16B_SLOW_SLICE -c "$here/coredemo.c" -o "$work/slow.o"
arm-none-eabi-gcc "${flags[@]}" -c "$here/cdsleep.c" -o "$work/sleep.o"
PYTHONPATH="$tool" python3 - "$work" "$out" "$tag" "$prefix" <<'PY'
from pathlib import Path
import sys
import xapp_v2_pack as packer
work, out = map(Path, sys.argv[1:3]); tag, prefix = sys.argv[3:5]
symbols = packer.kernel_symbols(out / 'kernel8-32.elf')
fp = packer.kernel_fingerprint(symbols, out / (tag + '.IMG'))
import os
kinds = None if os.environ.get('XDOS_XAPP_BOUND') == '1' else packer.kernel_symbol_kinds(out / 'kernel8-32.elf')
for src, name in (('core', 'CORE'), ('slow', 'SLOW'), ('sleep', 'SLEEP')):
    print(prefix + name + '.XAPP', packer.pack([work / (src + '.o')], symbols, fp, out / (prefix + name + '.XAPP'), kinds=kinds))
PY
