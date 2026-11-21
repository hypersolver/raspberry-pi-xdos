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
- **Shortcuts appear after the background** — The background draws first. Shortcuts wait for the FAT32 mount, manifest scan and executable checks. The first boot after an Ethernet upgrade also imports missing manifests. Individual stage timings have yet to be measured.
- **MP4 has no sound** — The video player handles H.264 video. Use the MP3 Player for audio files.
- **Stutter or VCHIQ warnings** — Try lower-resolution H.264 and record the UART playback counters.
- **Incompatible XAPP** — Use the matched kernel and XAPPs from this release, and verify their checksums.
