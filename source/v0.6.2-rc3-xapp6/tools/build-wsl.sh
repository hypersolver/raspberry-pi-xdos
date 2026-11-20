#!/usr/bin/env bash
set -euo pipefail

project_root=${1:?Project path is required}
archive_name=XDOS_ARM32_V0.6.2_RC3_PI3_USB_MSC_TESTKIT
circle_commit=7a81e1b480fe7d19fc1fc695b89bff0a0848f44b
image_name=xdos32-v062-rc3-pi3.img
deployment_name=xdos32-v062-rc3-xapp6-perf7-pi3.img
scroll_mode=${XDOS_SCROLL_MODE:-}
vc4_lvgl=${XDOS_VC4_LVGL:-0}
vc4_scroll_cache=${XDOS_VC4_SCROLL_CACHE:-0}
analytics_mode=${XDOS_ANALYTICS_MODE:-}
test_debug=${XDOS_TEST_DEBUG:-0}
case "$test_debug" in
    0|1) ;;
    *) printf 'XDOS_TEST_DEBUG must be 0 or 1\n' >&2; exit 1 ;;
esac
case "$analytics_mode" in
    "") ;;
    analyticsbase|analyticscache|analyticsregion) vc4_scroll_cache=1 ;;
    *) printf 'XDOS_ANALYTICS_MODE must be analyticsbase, analyticscache or analyticsregion\n' >&2; exit 1 ;;
esac
if [[ "$vc4_scroll_cache" == 1 ]]; then
    vc4_lvgl=1
fi
if [[ "$vc4_lvgl" == 1 ]]; then
    if [[ -n "$scroll_mode" && "$scroll_mode" != neon ]]; then
        printf 'XDOS_VC4_LVGL requires the neon baseline\n' >&2
        exit 1
    fi
    scroll_mode=neon
fi
case "$scroll_mode" in
    "") ;;
    baseline|vsync|optimized|reference|neon) deployment_name="xdos32-v062-rc3-xapp6-perf7-$scroll_mode-pi3.img" ;;
    *) printf 'XDOS_SCROLL_MODE must be baseline, vsync, optimized, reference or neon\n' >&2; exit 1 ;;
esac
if [[ "$vc4_lvgl" == 1 ]]; then
    deployment_name=xdos32-v062-rc3-xapp6-perf7-vc4-egl-pi3.img
fi
if [[ "$vc4_scroll_cache" == 1 ]]; then
    deployment_name=xdos32-v062-rc3-xapp6-perf7-vc4-cache-pi3.img
fi
if [[ -n "$analytics_mode" ]]; then
    deployment_name="xdos32-v062-rc3-xapp6-perf7-$analytics_mode-pi3.img"
fi
if [[ ${XDOS_BROWSER_XAPP:-0} == 1 ]]; then
    deployment_name="${deployment_name%-pi3.img}-browser-pi3.img"
fi

if [[ ${2:-} == --install-dependencies ]]; then
    sudo apt-get update
    sudo apt-get install -y build-essential git unzip ca-certificates python3 \
        gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi
fi

for tool in git make unzip python3 arm-none-eabi-gcc arm-none-eabi-g++ arm-none-eabi-objcopy arm-none-eabi-nm; do
    command -v "$tool" >/dev/null || {
        printf 'Missing %s. Run build-wsl.ps1 -InstallDependencies.\n' "$tool" >&2
        exit 1
    }
done

archive="$project_root/$archive_name.zip"
test -f "$archive"
run_id="$(date -u +%Y%m%dT%H%M%SZ)-$$"
output="$project_root/wsl-build-output/$run_id"
work="$HOME/.local/share/rpiduino-build/$run_id"
mkdir -p "$output" "$work"
exec > >(tee "$output/build.log") 2>&1

echo "Build output: $output"
{
    printf 'Target: Raspberry Pi 3 ARM32\nExpected Circle commit: %s\n' "$circle_commit"
    printf 'XDOS_TEST_DEBUG=%s\n' "$test_debug"
    uname -m
    arm-none-eabi-gcc --version
    sha256sum "$archive"
} | tee "$output/build-info.txt"

unzip -q "$archive" -d "$work"
git init -q "$work/circle"
git -C "$work/circle" fetch --depth 1 https://github.com/rsta2/circle.git "$circle_commit"
git -C "$work/circle" checkout --detach FETCH_HEAD
git -C "$work/circle" submodule update --init --depth 1 addon/wlan/hostap addon/lvgl/lvgl
git -C "$work/circle" submodule status --recursive | tee -a "$output/build-info.txt"

(cd "$work/circle" && ./configure -r 3 -p arm-none-eabi- --kernel-max-size 4)
printf '#pragma once\n#define XDOS_TEST_DEBUG %s\n' "$test_debug" \
    > "$work/circle/include/circle/xdos_test_debug.h"
source_dir="$work/$archive_name"
# The RC3 include can resolve to upstream LVGL instead of Circle's CLVGL wrapper.
sed -i 's|^#include <lvgl/lvgl.h>$|#include "../../addon/lvgl/lvgl.h"|' \
    "$source_dir/source/95-xdos-arm32-dualnet/kernel.h"
# Circle does not link libc snprintf. Its LVGL library supplies the builtin equivalent.
sed -i 's/\bsnprintf(/lv_snprintf(/g' \
    "$source_dir/source/95-xdos-arm32-dualnet/xdos_lvgl.cpp" \
    "$source_dir/source/95-xdos-arm32-dualnet/xdos_benchmark.cpp"
python3 "$project_root/tools/rc3-uart/apply.py" "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_external_demos.py" \
    "$source_dir/source/95-xdos-arm32-dualnet" "$work/circle"
python3 "$project_root/tools/rc3-uart/apply_calculator.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_notepad.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_workspace.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_arduino_demos.py" \
    "$source_dir/source/95-xdos-arm32-dualnet" "$source_dir"
python3 "$project_root/tools/rc3-uart/apply_xapp_ram.py" \
    "$source_dir/source/95-xdos-arm32-dualnet" "$source_dir" "$work/circle"
python3 "$project_root/tools/rc3-uart/apply_boot_display_perf.py" \
    "$source_dir/source/95-xdos-arm32-dualnet" "$work/circle"
python3 "$project_root/tools/rc3-uart/apply_uart_shortcuts.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_uart_net_fetch.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_uart_921600.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
if [[ -n "$scroll_mode" ]]; then
    python3 "$source_dir/tools/patch-circle-gfx-p0.py" "$work/circle"
    python3 "$project_root/tools/rc3-uart/apply_scroll_present.py" \
        "$source_dir/source/95-xdos-arm32-dualnet" "$work/circle" "$scroll_mode"
fi
if [[ "$vc4_lvgl" == 1 ]]; then
    python3 "$project_root/tools/vc4/apply_lvgl_vc4.py" \
        "$source_dir/source/95-xdos-arm32-dualnet" "$work/circle"
    make -C "$work/circle/addon/linux" -j"${JOBS:-2}"
    make -C "$work/circle/addon/vc4/vchiq" -j"${JOBS:-2}"
    (cd "$work/circle/addon/vc4/interface" && ./makeall -j"${JOBS:-2}")
fi
if [[ "$vc4_scroll_cache" == 1 ]]; then
    python3 "$project_root/tools/vc4/apply_vc4_scroll_cache.py" \
        "$source_dir/source/95-xdos-arm32-dualnet" "$work/circle"
fi
if [[ -n "$analytics_mode" ]]; then
    export XDOS_ANALYTICS_MODE="$analytics_mode"
    python3 "$project_root/tools/vc4/apply_analytics_cache.py" \
        "$source_dir/source/95-xdos-arm32-dualnet" "$work/circle" "$analytics_mode"
fi
if [[ ${XDOS_BROWSER_XAPP:-0} == 1 ]]; then
    python3 "$project_root/tools/rc3-uart/apply_browser_xapp.py" \
        "$source_dir/source/95-xdos-arm32-dualnet"
fi
python3 "$project_root/tools/rc3-uart/remove_xds_benchmark.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_xapp_windows.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/remove_update_xapp.py" \
    "$source_dir/source/95-xdos-arm32-dualnet"
python3 "$project_root/tools/rc3-uart/apply_text_keys.py" \
    "$source_dir/source/95-xdos-arm32-dualnet" "$work/circle"
python3 "$project_root/tools/rc3-uart/build_arduino_demos.py" prepare "$source_dir" "$work/circle"
bash "$project_root/tools/rc3-uart/build_calculator.sh" "$source_dir"
CIRCLE_HOME="$work/circle" bash "$source_dir/build-on-rpi4.sh"
python3 "$project_root/tools/rc3-uart/build_arduino_demos.py" finish "$source_dir" "$work/circle"

(
    cd "$source_dir/output"
    sha256sum -c "$image_name.sha256"
)
cp "$source_dir/output/$image_name" "$output/$deployment_name"
cp "$work/circle/sample/103-xdos-pi3-usb-msc/kernel8-32.elf" "$output/"
if arm-none-eabi-nm "$output/kernel8-32.elf" |
    grep -E ' (lv_demo_widgets|lv_demo_benchmark)$' >/dev/null; then
    echo "An official LVGL demo is still linked into the kernel." >&2
    exit 1
fi
for app in CALC NOTE PAINT FILES BROWSER LVGLDEMO LVGLBENCH; do
    cp "$source_dir/output/$app.APP" "$source_dir/output/$app.XAPP" "$output/"
done
(
    cd "$output"
    sha256sum "$deployment_name" > SHA256SUMS.txt
    sha256sum -c SHA256SUMS.txt
)
printf 'Verified image: %s/%s\n' "$output" "$deployment_name"
