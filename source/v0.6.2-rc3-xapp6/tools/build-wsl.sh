#!/usr/bin/env bash
set -euo pipefail

project_root=${1:?Project path is required}
archive_name=XDOS_ARM32_V0.6.2_RC3_PI3_USB_MSC_TESTKIT
circle_commit=7a81e1b480fe7d19fc1fc695b89bff0a0848f44b
image_name=xdos32-v062-rc3-pi3.img
deployment_name=xdos32-v062-rc3-xapp6-pi3.img

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
for app in CALC NOTE PAINT FILES BROWSER UPDATE LVGLDEMO LVGLBENCH; do
    cp "$source_dir/output/$app.APP" "$source_dir/output/$app.XAPP" "$output/"
done
(
    cd "$output"
    sha256sum "$deployment_name" > SHA256SUMS.txt
    sha256sum -c SHA256SUMS.txt
)
printf 'Verified image: %s/%s\n' "$output" "$deployment_name"
