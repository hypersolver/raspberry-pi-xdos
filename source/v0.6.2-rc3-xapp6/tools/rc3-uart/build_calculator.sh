#!/usr/bin/env bash
set -euo pipefail
tools="$(cd "$(dirname "$0")" && pwd)"
kit=${1:?XDOS kit directory required}
output="$kit/output"
mkdir -p "$output"
arm-none-eabi-gcc -marm -mcpu=cortex-a53 -c "$tools/calculator-header.S" \
    -o "$output/xapp-header.o"

build_module() {
    local source="$1" name="$2" relocations
    shift 2
    arm-none-eabi-gcc -std=c11 -marm -mcpu=cortex-a53 -Os -fpie -ffreestanding \
        -fno-stack-protector -fno-unwind-tables \
        -I"$kit/source/95-xdos-arm32-dualnet" "$@" -c "$tools/$source.c" \
        -o "$output/$name.o"
    relocations="$(arm-none-eabi-readelf -rW "$output/$name.o")"
    if grep -Eq 'R_ARM_(ABS32|TARGET1|MOVW_ABS_NC|MOVT_ABS|GOT_BREL)' <<<"$relocations"; then
        printf '%s contains an unsupported absolute relocation.\n' "$name" >&2
        return 1
    fi
    arm-none-eabi-ld -T "$tools/calculator.ld" -nostdlib -o "$output/$name.elf" \
        "$output/xapp-header.o" "$output/$name.o"
    arm-none-eabi-objcopy -O binary "$output/$name.elf" "$output/$name.XAPP"
    python3 "$kit/check_xapp.py" "$output/$name.XAPP"
    cp "$tools/$name.APP" "$output/$name.APP"
}

build_module calculator CALC
build_module notepad NOTE
build_module paint PAINT
build_module workspace FILES -DPANEL=1
build_module workspace BROWSER -DPANEL=2
build_module workspace UPDATE -DPANEL=3
