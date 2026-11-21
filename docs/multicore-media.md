# XDOS V0.6.2-RC3 XAPP6 MultiCore Media · Pi 3B+

[繁體中文](multicore-media.zh-TW.md) · [Release downloads](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-multicore-media)

## Changes and hardware verification

This build extends MultiCore APPSCAN with desktop shortcuts for Terminal, MP4 Player, and MP3 Player. It skips the old UPDATE manifest and retains the UART kernel update commands.

The released `MCMEDNET.IMG` booted on a Pi 3B+ at 1920×1080 after Ethernet deployment, CRC32 checks, and reset. Hardware testing confirmed UPDATE was absent, Terminal opened from the desktop, the MP4 displayed moving video, and the MP3 produced HDMI audio.

The hardware decoder presented all 120 frames of the 320×240 H.264 clip and returned `PLAY DONE`. VCHIQ `WARN_ON` messages remained, and frame interval P95 was in the 245 ms bucket. Playback smoothness remains open. Analog audio was outside this round of testing.

## Install, operate, and roll back

1. Verify the release downloads against `SHA256SUMS.txt`. Back up the card and `config.txt`, then extract the SD ZIP to a FAT32 partition with Pi 3B+ boot firmware.
2. Set one active `kernel=MCMEDNET.IMG` in `config.txt`, with `arm_64bit=0`. Keep the existing firmware settings and previous kernel.
3. Launch the players from the desktop. Use **PLAY** for H.264 MP4 video and **HDMI** or **3.5 MM** for MP3 or supported PCM WAV audio. Use `XDOS/VIDEO/`, `XDOS/MUSIC/`, or `XDOS/INBOX/` for your files. Sample media is supplied by the user.
4. For Ethernet updates, use the wired network and Debug UART at 921600 baud, 8N1. Upload the six paired XAPPs and the ten aliases in the manifest ZIP's `Network/` folder. Upload the IMG, select it as the boot image, and reset. Existing app manifests are preserved.
5. Keep MCASCAN and its paired applications for rollback. Use `boot-clear` and `reset` if MCASCAN was active before the upgrade, or restore the previous `kernel=` entry offline.

## Troubleshooting

- **Missing shortcut** — Verify the manifest and executable paths, the ten-shortcut limit, and reboot to rescan.
- **Shortcuts appear after the background** — The background draws first. Shortcuts wait for USB and SD initialization, the FAT32 mount, manifest scan and executable checks. See the timings below.
- **MP4 has no sound** — The video player handles H.264 video. Use the MP3 Player for audio files.
- **Stutter or VCHIQ warnings** — Try lower-resolution H.264 and record the UART playback counters.
- **Incompatible XAPP** — Use the matched kernel and XAPPs from this release, and verify their checksums.

## Icon startup experiment

On the same Pi 3B+ and SD card, one reset per build measured the first icon draw at 5.981 s in the baseline and 5.620 s in ICONFAST, about 362 ms earlier. SD initialization took 1.536 s, FAT32 mount took under 1 ms, and the catalog scan took about 49 ms. These times use the kernel clock and software draw marker. The change runs SD initialization and catalog scanning before the deferred Ethernet PHY probe. Terminal opened and closed on ICONFAST. The published release remains the hardware-tested MCMEDNET build.

For a diagnostic rebuild, run `tools/vc4/apply_icon_boot.py SAMPLE --fast --profile` on the generated MCMEDNET source tree, then rebuild and rebind its paired XAPPs. Omit `--profile` to disable timing output.

## SD polling and icon drawing order

SDPRE checks SD card readiness every 20 ms while ACMD41 reports busy. ICONFAST waits 500 ms between checks. SDPRE draws the icons after FAT32 and the catalog are ready, then enumerates USB. Both drivers still initialize on the main core. [SD Association specification](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver7.10.pdf)

On the same Pi 3B+, card and 1080p setup, one reset per build measured SD initialization at 1.536 s in ICONFAST and 0.096 s in SDPRE. Icon first draw advanced from 5.620 s to 1.478 s. USB readiness moved from 3.886 s to 4.186 s. UART confirmed mouse enumeration and opening and closing Terminal, Browser, Widgets and Benchmark. ICONFAST remains available for rollback.

To reproduce SDPRE, apply `apply_icon_boot.py SAMPLE --fast --profile` followed by `apply_sd_icon_boot.py CIRCLE SAMPLE` in an isolated build tree. Rebuild the SDCard library and kernel, then rebind the paired XAPPs.
