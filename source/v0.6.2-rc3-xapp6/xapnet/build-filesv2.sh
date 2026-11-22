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
arm-none-eabi-nm -u "$work/files.o" "$work/notepad.o" "$work/calculator.o" "$work/paint.o" "$work/terminal.o" "$work/video.o" "$work/music.o" "$work/browser.o" "$work/widgets.o" "$work/benchmark.o" | awk '$1=="U" {print "LDFLAGS += --undefined=" $2}' | sort -u >> "$sample/xapp-imports.mk"
cp "$src"/circle-net/*.cpp "$circle/lib/net/"
make -C "$circle/lib/net" -j4
make -C "$sample" -j4
cp "$sample/kernel8-32.img" "$out/FVMNET2.IMG"
cp "$sample/kernel8-32.elf" "$out/kernel8-32.elf"
PYTHONPATH="$tool" "${PACK_PYTHON:-python3}" - "$work" "$out" <<'PY'
from pathlib import Path
import sys
import xapp_v2_pack as packer
work,out=map(Path,sys.argv[1:])
symbols=packer.kernel_symbols(out/'kernel8-32.elf')
fp=packer.kernel_fingerprint(symbols,out/'FVMNET2.IMG')
for source,target in [('files','N9FILES.XAPP'),('notepad','N9NOTE.XAPP'),('calculator','N9CALC.XAPP'),('paint','N9PAINT.XAPP'),('terminal','N9TERM.XAPP'),('video','N9VIDEO.XAPP'),('music','N9MUSIC.XAPP'),('browser','N9BROWSE.XAPP'),('widgets','N9WIDGET.XAPP'),('benchmark','N9BENCH.XAPP')]:
    packer.SLOT_BYTES=(768 if source=='benchmark' else 320)*1024
    print(target, packer.pack([work/(source+'.o')],symbols,fp,out/target))
PY
(cd "$out" && sha256sum FVMNET2.IMG N9FILES.XAPP N9NOTE.XAPP N9CALC.XAPP N9PAINT.XAPP N9TERM.XAPP N9VIDEO.XAPP N9MUSIC.XAPP N9BROWSE.XAPP N9WIDGET.XAPP N9BENCH.XAPP > SHA256SUMS.txt)
