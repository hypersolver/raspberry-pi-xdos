# FVR20 for Raspberry Pi 3B+

FVR20 moves the debug console on GPIO14/15 to the mini UART and leaves PL011 available. The host console still uses 921600 baud, 8N1. The firmware settings pin the VideoCore core clock to 400 MHz. At that clock the mini UART divisor yields about 925925 baud, 0.47% above the requested rate. The change also adds `FILE LIST <path>` and waits for transmit-buffer space during long UART responses.

## Install and roll back

Verify the IMG and XAPPs against SHA256SUMS.txt. FVR20 uses the same ten J7 XAPPs as FVR19. Follow the [deployment guide](OPERATIONS_ZH.md) for network or microSD installation. Confirm the Pi's current address before selecting the host bind address. Back up config.txt, deploy FVR20.IMG, select it with BOOT and send RESET. Allow for one additional restart if the firmware clock settings need updating.

To roll back, select the preserved FVR19.IMG with BOOT and RESET, then verify the FVR19 banner and UART PING. The clock settings persist across a kernel-only rollback. To revert those settings, use the original config backup and verify that its kernel entry selects FVR19.IMG before rebooting. If the UART cannot be reached, edit the boot microSD on the PC.

## Hardware validation

On Pi 3B+, a 256 KiB UART upload passed CRC32 in 22.8 seconds. A fresh 256 KiB Ethernet upload passed FETCH and CRC32, four directory listings were complete, and the core clock stayed at 400 MHz during the 60-second observation. FVR19 rebooted and replied to PING after rollback.
