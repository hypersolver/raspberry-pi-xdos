# XDOS V0.6.2-RC3 XAPP6 MultiCore APPSCAN · Pi 3B+

[繁體中文完整操作手冊](multicore-appscan.zh-TW.md)

This release combines the wired Pi 3B+ MultiCore base with APPSCAN dynamic desktop shortcuts. The first ten valid manifests occupy a two-by-five grid. Labels are centered and hidden above 16 display characters. Icon letters and colors follow the [desktop specification](xapp-desktop-spec.md).

## Install and operate

[Release downloads](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-multicore-appscan) · [Build sources](https://github.com/hypersolver/raspberry-pi-xdos/releases/download/v0.6.2-rc3-xapp6-multicore-appscan/XDOS-MultiCore-APPSCAN-source.zip)

1. Verify the release assets with `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash -Algorithm SHA256`.
2. Prepare a FAT32 boot partition with Pi 3B+ firmware. Back up the card and `config.txt`. Extract the SD ZIP to the partition root.
3. Set `kernel=MCASCAN.IMG` and `arm_64bit=0` while preserving the applicable firmware settings. Power on with HDMI and USB input connected. Click an icon or its label. Close windows with **X** and Arduino demos with **BACK TO DESKTOP**.
4. For Ethernet updates, use the existing UART-triggered `net-scp` flow at 921600 baud. Stage the four paired XAPPs as `ASWIDGET.XAPP`, `ASBENCH.XAPP`, `ASBROWSE.XAPP` and `ASTERM.XAPP`. Stage the core, check CRC32, then select it and reboot. Copy missing manifests or their executable files from the SD ZIP to `XDOS/APPS/` first.

## Troubleshooting and engineering record

- **Missing shortcut** — Check `.APP` and its executable path, then reboot to rescan. The first ten valid entries fill the desktop.
- **Missing label** — Shorten `Name=` to 16 display characters or fewer.
- **Incompatible XAPP** — Check the release, filename and SHA-256 of the matching kernel and XAPP.
- **Rollback** — Use `boot-clear` followed by `reset`, or restore the previous `config.txt` offline.

QA3 on Pi 3B+ at 1080p verified eleven manifests yield ten shortcuts, a centered 16-character label, and launch via an icon with a hidden 17-character label. The release image scans the normal apps directory. The release kernel and four paired XAPPs passed Ethernet CRC32 checks. After reset, the release booted at 1080p and UART opened and closed Widgets, Benchmark, Browser and Terminal. Historical benchmark measurements are available in the [MultiCore base release notes](../source/v0.6.2-rc3-xapp6/MULTICORE_EN.md).

Arduino IDE one-click XAPP install, standalone media-demo hardware validation, exFAT/NTFS, and Pi 4/5 XDOS XAPP runtime support remain separate work items.
