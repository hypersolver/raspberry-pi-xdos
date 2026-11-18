# XD OS x86 Assembly → ARM32 Assembly port map

Upstream: https://github.com/eklavya-raj/xdos (MIT)

**Target:** Raspberry Pi 3 Model B, Cortex-A53, 32-bit ARM state, Circle bare metal.

The port is at **assembly source / control-flow level**, not binary recompilation. Original x86 PC platform setup does not carry over. Circle handles Raspberry Pi hardware. ARM assembly is used for selected original XD OS GUI routines, and C++ remains only an interim application/event shell until further x86 routines are ported.

| Original file / x86 routine | ARM file / routine | What is preserved | What intentionally differs |
|---|---|---|---|
| `kernel/kernel.asm` lines ~925–974 `clip` / `rect` | `xdos_gui.S`: `xdos_arm_rect` | clip coordinates, row-by-row rectangle fill | supports both RGB565 and XRGB8888, uses runtime GPU pitch instead of fixed `WIDTH * 4` |
| `kernel/kernel.asm` lines ~1183–1214 `glyph` | `xdos_gui.S`: `xdos_arm_glyph` | 16 scanlines, MSB-first per-pixel bitmap test, clipping | font decoder provisionally provided by Circle; text width fixed at 8px rather than upstream variable-width `FW` |
| `kernel/kernel.asm` `press` / hit-test path | `xdos_gui.S`: `xdos_arm_hit_window` | rectangle bounds/visibility check | uses 6-window interim desktop struct rather than upstream 9-window `wins[]` + `zord[]` model |
| `kernel/kernel.asm` lines ~338–355 `dragging` | `xdos_gui.S`: `xdos_arm_drag_window` | mouse position minus grab offset, update window x/y, clamp y ≥ 0 | additionally clamps x/y to framebuffer bounds |

## How the ARM32 code is called

AAPCS32 calling convention: arguments 0–3 in r0–r3; remaining arguments on the stack; return in r0; registers r4–r11 and SP preserved by assembly functions. All functions are exported with C linkage in `asm_gui.h` and invoked from `desktop.cpp`.

The assembly functions directly write the framebuffer; there are **no Linux syscalls** and no NEON requirements. All framebuffer memory stores use `pitchBytes` from the firmware instead of assuming packed rows.

## What is not yet converted

- Full upstream 9-window state and z-order table, variable-width fonts, titlebar design, menu code
- Original `kernel/apps/apps.inc` user applications and terminal semantics
- `kernel/apps/browser.inc` networking / browser UI
- `kernel/net/*` low-level NIC, TCP/IP, TLS
- x86 V86 emulation in `kernel/vm.inc` (needs an interpreter / full guest CPU emulator, *not* instruction-for-instruction execution)
- BIOS bootloader and legacy PC keyboard/mouse drivers (replaced by Circle on Pi 3B)

## Integrity and verification

1. `xdos_gui.S` has been assembled with `clang --target=armv7a-none-eabi -mcpu=cortex-a53 -marm` and produces an ARM ELF32 relocatable EABI object.
2. A C++ declaration/structure layout check prevents silent ABI drift of the `Window` fields.
3. **Full Circle link on the user's RPi4 and actual RPi3B HDMI+USB joint operation are not yet verified.** Follow README for that test.
4. The old XD OS-style V0.2 C++ desktop is retained as an interim GUI harness. This is *not* a complete faithful port of the original XD OS yet.

## Next direct-translation milestones

1. Rebuild original `wins[]`, `zord[]`, `dirty`, `appdraw[]` data model in ARM32 assembly.
2. Port `scene`/`window`/`grad`/`shade`/`text`, retaining original layout.
3. Port original `mouse` dispatch and app callbacks with Circle HID event input.
4. Port individual upstream apps, then storage/network, last the game compatibility layer.

Licenses: XD OS upstream MIT; Circle GPL-3.0-or-later. If distributing linked binaries, comply with applicable Circle GPL obligations; keep attribution and license texts appropriate to the distribution.
