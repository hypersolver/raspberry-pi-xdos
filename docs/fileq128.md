# FILEQ128 for Raspberry Pi 3B+

This prerelease includes the twelve-shortcut desktop, resolution-aware power
menu, XAPP taskbar and window controls. Arduino Files adds a scrollable SD/USB
directory tree and file list, navigation history, text previews and XAPP
launch. The host reads at most 128 entries per page. Large folders can be
browsed with Prev and Next. Arduino Image View supports uncompressed 24-bit
and 32-bit BMP.

## Development and validation

The Pi 3B+ booted FILEQ128 at 1920×1080 after LAN deployment, and the core and
paired XAPPs passed CRC32. UART opened Files and responded to PING afterward.
Physical scroll-wheel behavior, large-directory pagination and BMP rendering
still need HDMI checks.

Windows Arduino CLI built Basic, LED Blink, LVGL Click Counter and Image View
as XAPPs with RPiDuino 0.1.4. Standalone Blink built as an IMG. XAPP upload
was checked with a simulated receiver. IDE hardware upload, migration of the
remaining apps and four-XAPP scheduling are still pending.

## Installation, update and recovery

1. Verify the SHA-256 manifest. Extract the SD overlay onto an existing Pi 3B+
   FAT32 boot partition with the Pi boot firmware. For a new card, follow the
   installation guide to add the boot firmware first.
2. Back up config.txt and change its active kernel setting to
   kernel=FILEQ128.IMG. Keep the previous kernel and paired files.
3. Open Arduino Files. Select a folder to enter it or select a compatible
   .XAPP to launch it. Q1IMAGE.XAPP is in XDOS/INBOX.
4. To update over Ethernet, stage paired XAPPs and the IMG using UART
   net-scp. Select the new IMG with BOOT and issue RESET. Use the SD overlay
   to install missing desktop manifests or replace a file already on the card.
5. In Arduino IDE, select RPiDuino 3/3B+ and Build target → XDOS XAPP. Use
   the bundled FILEQ128 host descriptor and the debug UART port.

If XDOS rejects an XAPP, verify its kernel pairing. If a USB folder is
unavailable, wait for the FAT32 USB volume to mount. The desktop shows up to
twelve valid app manifests. On shutdown, loss of HDMI signal is still pending
verification. To roll back, select the previous IMG over UART or restore the
saved config.txt on the microSD.

[Installation guide](../source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) ·
[Arduino setup](../Arduino%20SDK/XDOS-SAMPLES.md) ·
[繁體中文](fileq128.zh-TW.md)
