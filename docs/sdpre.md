# XDOS SDPRE · Raspberry Pi 3B+

[繁體中文](sdpre.zh-TW.md) · [Release downloads](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-sdpre)

## Changes and hardware verification

SDPRE reduces the SD card's ACMD41 polling interval from 500 ms to 20 ms and draws the icons before USB enumeration. USB and SD initialization still run on the main core.

| Kernel timing | ICONFAST | SDPRE |
| --- | ---: | ---: |
| SD initialization | 1.536 s | 0.096 s |
| First icon draw | 5.620 s | 1.478 s |
| USB ready | 3.886 s | 4.186 s |

On the same Pi 3B+ and microSD at 1080p, the first icon draw was **4.142 s earlier** and USB was ready **about 300 ms later**. Values are from one reset per build and the kernel's software draw marker. Terminal, Browser, Widgets and Benchmark opened and closed over UART. A physical mouse opened Terminal, and a physical keyboard entered and ran `HELP`.

## Install and operate

1. Verify the release downloads with SHA-256. Extract the SD ZIP to a FAT32 boot partition with Pi 3B+ firmware. Back up the existing card and `config.txt` first.
2. Keep the existing firmware settings. Set one active `kernel=SDPRE.IMG` and `arm_64bit=0`. Eject the card, insert it into the Pi, connect HDMI and USB input, and power on.
3. Click an icon or its label to open an app. The full SD ZIP includes the `.APP` manifests and their executables. This release uses `SPWIDGET.XAPP`, `SPBENCH.XAPP`, `SPBROWSE.XAPP` and `SPTERM.XAPP` as the kernel-bound module names. `MNMP4.XAPP` and `MNMP3.XAPP` retain the Media release's hardware-independent XAPP header and tested filenames.

## Ethernet update and rollback

On an existing MCMEDIA card, use Ethernet with Debug UART at 921600 baud, 8N1. Use `net-scp` to stage the four `SP*.XAPP` files and `SDPRE.IMG` under their original names. Check CRC32 for each transfer, then run `boot SDPRE.IMG --name SDPRE.IMG` and `reset`. Keep the existing MP4 and MP3 modules and manifests. For an empty or incomplete card, use the full SD ZIP.

Keep `ICONFAST.IMG` and its `IF*.XAPP` modules. When ICONFAST was the previously selected kernel, `boot-clear` and `reset` restore it. Alternatively, power down and set `kernel=ICONFAST.IMG` in `config.txt`. [UART guide](../source/v0.6.2-rc3-xapp6/UART_ZH.md)

## Troubleshooting

- **Icons appear before the mouse works** — Wait for USB enumeration. The measured delay from first icon draw to USB readiness is about 2.7 s.
- **An app is missing** — Check `XDOS/APPS/`, the manifest's executable path and the ten-shortcut limit. Reboot after editing the catalog.
- **An app reports a compatibility error** — Verify the IMG and matching XAPP names and checksums.
- **MP4 stutters or logs VCHIQ warnings** — The prior Media playback test completed but logged warnings. Playback smoothness was outside the SDPRE acceptance test. [MCMEDIA playback notes](multicore-media.md)

## USB and SD boot overlap experiment

USBOV2 preserved 170 ms of root-port waits, 1,020 ms across two hub power waits, and 400 ms across four hub port reset waits. The rest of USB initialization was not broken down further. The initial experiment stopped when a background task called VC4 rendering. USBOV2 prepares storage in the background and renders on the main task.

On the same Pi 3B+, icon first draw moved from 1.478 s to 2.297 s and USB readiness from 4.186 s to 4.316 s. The mouse and keyboard attached at about 4.317 s in USBOV2. Terminal, Browser, Widgets and Benchmark opened and closed over UART. PWRMENU retains SDPRE's initialization order. In its boot test, icons drew at 1.494 s and USB was ready at 4.448 s.

## PWRMENU power menu

Click **XDOS** at the bottom left to open the upward menu. Choose **Restart** or **Shut Down**, or press the underlined **R** or **S** while the menu is open. Press Esc or click outside to dismiss it. Active storage writes or update transfers block power actions. On Pi 3B+, shutdown halts the system and the external power remains connected.
