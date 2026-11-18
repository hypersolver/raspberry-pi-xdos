# V0.3 changes compared with the booted V0.2 DEBUG
- Keep AArch32 ASM drawing and dragging; do not replace with C GUI.
- Add `CNetSubSystem` + `CScheduler` + `CWebTask` backed by LAN9514 USB Ethernet.
- Browser input text field, HTTP-only URL, DNS + GET, bounded plain-text parser.
- New back framebuffer and partial rectangle blits; single cursor overlay.
- Network waits for DHCP without stalling HDMI desktop.
- New build directory `sample/92-xdos-arm32-net` so confirmed-good V0.2 is untouched.
- Output name `xdos32-v03.img`; use matching MicroSD `kernel=` setting.

Validation: source/code review and toolchain preflight only; actual Pi4 Circle link
and Pi3B USB/Ethernet/browser/anti-flicker verification are not yet completed.
