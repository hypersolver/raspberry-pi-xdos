#!/usr/bin/env bash
set -euo pipefail

base=$1
deps=$2
project=$(cd "$(dirname "$0")/../.." && pwd)
circle="$base/circle"
kit="$base/XDOS_ARM32_V0.6.2_RC3_PI3_USB_MSC_TESTKIT"
sample=$(mktemp -d "$circle/sample/xdos-vc4-player.XXXXXX")
player_kit=$(mktemp -d "$base/player-kit.XXXXXX")
output="${XDOS_PLAYER_OUTPUT:-"$project/wsl-build-output/vc4-player"}"
module_tools="$project/tools/rc3-uart"
prefix=$3
[[ $prefix =~ ^[A-Z0-9]{2}$ ]]

mkdir -p "$player_kit/output" "$player_kit/source" "$output"
cp -a "$circle/sample/103-xdos-pi3-usb-msc/." "$sample/"
cp -a "$kit/output/arduino-demos" "$player_kit/output/"
ln -s "$sample" "$player_kit/source/95-xdos-arm32-dualnet"
python3 "$project/tools/vc4/apply_player.py" "$sample" "$deps"
python3 "$module_tools/apply_pixel_bbs.py" "$sample"
if [[ ${XDOS_DESKTOP_APPS:-1} == 1 ]]; then
    python3 "$module_tools/apply_desktop_apps.py" "$sample"
fi
sed -i "s@/INBOX/F8@/INBOX/$prefix@g" "$sample/app_shortcut.cpp"
make -C "$sample" -j4 XDOS_BUFFER_ROWS=128

image=xdos32-v062-rc3-pi3.img
cp "$sample/kernel8-32.img" "$player_kit/output/$image"
XDOS_BROWSER_XAPP=1 python3 "$module_tools/build_arduino_demos.py" \
    finish "$player_kit" "$circle" "$sample/kernel8-32.elf"

arm-none-eabi-gcc -marm -mcpu=cortex-a53 -c "$module_tools/calculator-header.S" \
    -o "$player_kit/player-header.o"
arm-none-eabi-gcc -std=c11 -marm -mcpu=cortex-a53 -Os -fpie -ffreestanding \
    -fno-stack-protector -fno-unwind-tables -I"$sample" \
    -c "$project/tools/vc4/player_xapp.c" -o "$player_kit/player.o"
arm-none-eabi-ld -T "$module_tools/calculator.ld" -nostdlib \
    "$player_kit/player-header.o" "$player_kit/player.o" -o "$player_kit/player.elf"
arm-none-eabi-objcopy -O binary "$player_kit/player.elf" "$output/PLAYER.XAPP"
python3 "$kit/check_xapp.py" "$output/PLAYER.XAPP"

cp "$sample/kernel8-32.img" "$output/VC4PLAY.IMG"
cp "$player_kit/output/LVGLDEMO.XAPP" "$output/${prefix}WIDGET.XAPP"
cp "$player_kit/output/LVGLBENCH.XAPP" "$output/${prefix}BENCH.XAPP"
cp "$player_kit/output/BROWSER.XAPP" "$output/${prefix}BROWSE.XAPP"
cp "$player_kit/output/TERMINAL.XAPP" "$output/${prefix}TERM.XAPP"
(cd "$output"; sha256sum VC4PLAY.IMG PLAYER.XAPP "$prefix"*.XAPP > SHA256SUMS.txt; sha256sum -c SHA256SUMS.txt)

if [[ ${XDOS_DESKTOP_APPS:-1} == 1 ]]; then
    apps="$output/XDOS/APPS"
    inbox="$output/XDOS/INBOX"
    mkdir -p "$apps" "$inbox"
    # The fixed shortcuts still use their specialized host APIs.
    for app in NOTE CALC PAINT FILES; do
        cp "$kit/output/$app.APP" "$kit/output/$app.XAPP" "$apps/"
    done
    for app in LVGLDEMO LVGLBENCH BROWSER TERMINAL; do
        cp "$player_kit/output/$app.APP" "$player_kit/output/$app.XAPP" "$apps/"
    done
    printf '[Application]\nName=MP4 Player\nExec=SD:/XDOS/INBOX/PLAYER.XAPP\nABI=1\nArch=armv7-a32\n' > "$apps/PLAYER.APP"
    cp "$output/PLAYER.XAPP" "$output/${prefix}WIDGET.XAPP" "$output/${prefix}BENCH.XAPP" \
       "$output/${prefix}BROWSE.XAPP" "$output/${prefix}TERM.XAPP" "$inbox/"
    (cd "$output"; find XDOS -type f -print0 | sort -z | xargs -0 sha256sum >> SHA256SUMS.txt; sha256sum -c SHA256SUMS.txt)
fi
