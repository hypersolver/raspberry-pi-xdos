#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 2 ]]; then
    echo "Usage: build-fileq128.sh <prepared SDPRE Circle directory> <output directory>" >&2
    exit 2
fi
original=$(realpath "$1")
out=$(realpath -m "$2")
repo=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
from="$original/sample/xdos-sdboot/kernel.cpp"
circle="$work/circle"
mkdir "$circle"
for item in addon include lib sample Rules.mk Config.mk circle.ld; do
    if [[ "$item" == sample ]]; then
        mkdir "$circle/sample"
        cp -a "$original/sample/Rules.mk" "$circle/sample/"
    else
        cp -a "$original/$item" "$circle/"
    fi
done
for name in Config2.mk circle64.ld; do if [[ -f "$original/$name" ]]; then cp -a "$original/$name" "$circle/"; fi; done
d="$circle/sample/xdos-sdboot"
cp -a "$(dirname "$from")" "$d"
mkdir -p "$out"
python3 "$repo/tools/rc3-uart/apply_power_menu.py" "$d"
python3 "$repo/tools/rc3-uart/apply_files_tree.py" "$d"
sed -i 's/MCMediaSDPRE /MCMediaFILEQ128 /' "$d/kernel.cpp"
sed -i -E 's@/INBOX/SP(WIDGET|BENCH|BROWSE|TERM)\.XAPP@/INBOX/FQ\1.XAPP@g' "$d/app_shortcut.cpp"
stage=$(mktemp -d)
mkdir -p "$stage/output" "$stage/source"
ln -s "$d" "$stage/source/95-xdos-arm32-dualnet"
export XDOS_BROWSER_XAPP=1 XDOS_DESKTOP_SAMPLES=1
builder="$repo/tools/rc3-uart/build_arduino_demos.py"
python3 "$builder" prepare "$stage" "$circle"
{ make -C "$circle/addon/lvgl" -j6 && make -C "$d" -j6; } > "$out/build.log" 2>&1 || { tail -70 "$out/build.log"; exit 1; }
cp "$d/kernel8-32.img" "$out/FILEQ128.IMG"
cp "$d/kernel8-32.img" "$stage/output/xdos32-v062-rc3-pi3.img"
python3 "$builder" finish "$stage" "$circle" "$d/kernel8-32.elf"
cp "$stage/output/LEDBLINK.XAPP" "$out/FQBLINK.XAPP"
cp "$stage/output/ARDLVGL.XAPP" "$out/FQLVGL.XAPP"
cp "$stage/output/IMGVIEW.XAPP" "$out/Q1IMAGE.XAPP"
for app in WIDGET:LVGLDEMO BENCH:LVGLBENCH BROWSE:BROWSER TERM:TERMINAL; do
    cp "$stage/output/${app#*:}.XAPP" "$out/FQ${app%:*}.XAPP"
done
(cd "$out"; sha256sum *.IMG *.XAPP > SHA256SUMS.txt; sha256sum -c SHA256SUMS.txt)
