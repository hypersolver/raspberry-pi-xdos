#!/usr/bin/env bash
set -euo pipefail
base=$1
out=$2
# Build identity: kernel IMG name and paired XAPP prefix (defaults keep FVMNET2 / N9).
tag=${XDOS_BUILD_TAG:-FVMNET2}
prefix=${XDOS_APP_PREFIX:-N9}
src=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d "${base%/*}/filesv2.XXXXXX")
mkdir -p "$out"
echo "$work" > "$out/workdir.txt"
cp -a "$base/circle" "$work/circle"
circle=$work/circle
sample=$circle/sample/xdos-sdboot
cp "$src"/kernel/* "$sample/"
printf 'DEFINE += -DXDOS_BUILD_TAG=\\"%s\\" -DXDOS_APP_PREFIX=\\"%s\\"\n' "$tag" "$prefix" >> "$circle/Config.mk"
tool="$src/xappv2-tools"   # packer + export-table generator (R17), versioned with the kernel
flags=(-Os -mcpu=cortex-a53 -marm -mfloat-abi=hard -mfpu=neon-vfpv4 -ffreestanding -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti -mword-relocations -DRASPPI=3 -DAARCH=32 -DLV_CONF_INCLUDE_SIMPLE -I "$circle/addon/lvgl/lvgl" -I "$circle/addon/lvgl" -I "$circle/include" -I "$sample" -Wall -Wextra)
for app in files notepad calculator paint terminal; do
    arm-none-eabi-g++ "${flags[@]}" -c "$src/apps/$app.cpp" -o "$work/$app.o"
done
arm-none-eabi-g++ "${flags[@]}" -c "$src/apps/media.cpp" -o "$work/video.o"
arm-none-eabi-g++ "${flags[@]}" -DXDOS_MUSIC_APP -c "$src/apps/media.cpp" -o "$work/music.o"
arm-none-eabi-g++ "${flags[@]}" -I "$circle/addon" -c "$sample/webtask.cpp" -o "$work/webtask.o"
arm-none-eabi-g++ "${flags[@]}" -c "$src/apps/browser.cpp" -o "$work/browser-ui.o"
arm-none-eabi-ld -r -o "$work/browser.o" "$work/browser-ui.o" "$work/webtask.o"
mkdir -p "$work/demo-conf"
sed -E 's/(#define (LV_USE_DEMO_WIDGETS|LV_USE_DEMO_BENCHMARK|LV_FONT_MONTSERRAT_26) +)0/\11/' "$circle/addon/lvgl/lv_conf.h" > "$work/demo-conf/lv_conf.h"
demo_flags=(-I "$work/demo-conf" -I "$sample/xdos_arduino" "${flags[@]}" -Wno-unused-parameter)
mapfile -t widget_sources < <(find "$circle/addon/lvgl/lvgl/demos/widgets" -name '*.c' | sort)
mapfile -t benchmark_sources < <(find "$circle/addon/lvgl/lvgl/demos/benchmark" -name '*.c' | sort)
widget_objects=()
benchmark_objects=()
cflags=()
for flag in "${demo_flags[@]}"; do
    case "$flag" in -fno-rtti|-fno-exceptions) ;; *) cflags+=("$flag");; esac
done
for source in "${widget_sources[@]}" "${benchmark_sources[@]}" "$circle/addon/lvgl/lvgl/src/font/lv_font_montserrat_26.c"; do
    obj="$work/$(basename "$source" .c).o"
    arm-none-eabi-gcc "${cflags[@]}" -c "$source" -o "$obj"
    case "$source" in */demos/widgets/*) widget_objects+=("$obj");; *) benchmark_objects+=("$obj");; esac
done
for app in widgets benchmark; do
    if [[ $app == widgets ]]; then
        sketch=LVGLWidgetsDemo
        app_objects=("${widget_objects[@]}")
        extra=()
    else
        sketch=LVGLBenchmarkDemo
        app_objects=("${widget_objects[@]}" "${benchmark_objects[@]}")
        extra=(-DXDOS_BENCHMARK_SKETCH)
    fi
    arm-none-eabi-g++ "${demo_flags[@]}" -DRPIDUINO_LVGL_HOST_TIMER -x c++ -c "$src/../arduino-demos/$sketch/$sketch.ino" -o "$work/$app-sketch.o"
    arm-none-eabi-g++ "${demo_flags[@]}" "${extra[@]}" -c "$src/../tools/rc3-uart/sketch_xapp.cpp" -o "$work/$app-entry.o"
    arm-none-eabi-ld -r -o "$work/$app.o" "$work/$app-sketch.o" "$work/$app-entry.o" "${app_objects[@]}"
done
# R17: keep every name of the ABI export list in the kernel, used or not.
"${PACK_PYTHON:-python3}" "$tool/gen_exports.py" --keep "$src/abi-exports.txt" "$sample/xdos_abi_keep.c"
echo 'OBJS += xdos_abi_keep.o' >> "$sample/xapp-imports.mk"
arm-none-eabi-nm -u "$work/files.o" "$work/notepad.o" "$work/calculator.o" "$work/paint.o" "$work/terminal.o" "$work/video.o" "$work/music.o" "$work/browser.o" "$work/widgets.o" "$work/benchmark.o" | awk '$1=="U" {print "LDFLAGS += --undefined=" $2}' | sort -u >> "$sample/xapp-imports.mk"
cp "$src"/circle-net/*.cpp "$circle/lib/net/"
make -C "$circle/lib/net" -j4
cp "$src"/circle-lib/serial.cpp "$circle/lib/"
make -C "$circle/lib" -j4
cp "$src"/circle-lvgl/*.cpp "$circle/addon/lvgl/"   # R16-B: core lending limit, sleep/time API
make -C "$circle/addon/lvgl" -j4
make -C "$sample" -j4
# R17: second link with the export table, so kernel-independent XAPPs can bind
# their imports at load time. The table references every exported symbol.
"${PACK_PYTHON:-python3}" "$tool/gen_exports.py" "$sample/kernel8-32.elf" "$sample/xdos_exports.c" --require "$src/abi-exports.txt"
sed -i '/xdos_abi_keep.o/d' "$sample/xapp-imports.mk"   # the export table now keeps them
echo 'OBJS += xdos_exports.o' >> "$sample/xapp-imports.mk"
make -C "$sample" -j4
cp "$sample/kernel8-32.img" "$out/$tag.IMG"
cp "$sample/kernel8-32.elf" "$out/kernel8-32.elf"
PYTHONPATH="$tool" "${PACK_PYTHON:-python3}" - "$work" "$out" "$tag" "$prefix" <<'PY'
from pathlib import Path
import sys
import xapp_v2_pack as packer
work,out=map(Path,sys.argv[1:3])
tag,prefix=sys.argv[3:5]
symbols=packer.kernel_symbols(out/'kernel8-32.elf')
import os
# R17: kernel-independent modules by default; XDOS_XAPP_BOUND=1 packs bound ones as before.
kinds=None if os.environ.get('XDOS_XAPP_BOUND')=='1' else packer.kernel_symbol_kinds(out/'kernel8-32.elf')
fp=packer.kernel_fingerprint(symbols,out/(tag+'.IMG'))
for source,target in [('files',prefix+'FILES.XAPP'),('notepad',prefix+'NOTE.XAPP'),('calculator',prefix+'CALC.XAPP'),('paint',prefix+'PAINT.XAPP'),('terminal',prefix+'TERM.XAPP'),('video',prefix+'VIDEO.XAPP'),('music',prefix+'MUSIC.XAPP'),('browser',prefix+'BROWSE.XAPP'),('widgets',prefix+'WIDGET.XAPP'),('benchmark',prefix+'BENCH.XAPP')]:
    packer.SLOT_BYTES=(768 if source=='benchmark' else 320)*1024
    print(target, packer.pack([work/(source+'.o')],symbols,fp,out/target,kinds=kinds))
PY
(cd "$out" && sha256sum "$tag.IMG" "$prefix"*.XAPP > SHA256SUMS.txt)
