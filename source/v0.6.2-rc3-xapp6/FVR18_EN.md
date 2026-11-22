# FVR18 for Raspberry Pi 3B+

The desktop taskbar displays utilization percentages for CPU0–CPU3. While an XAPP is active, its name appears in the corresponding CPU column. Idle desktops still show the percentages. The ten J7 XAPPs are byte-for-byte identical to those in FVR17.

## Install and operate

1. Download `FVR18.IMG`, the ten J7 XAPPs if needed, and `SHA256SUMS.txt`. Verify with `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash -Algorithm SHA256`.
2. Use the [network and UART deployment guide](OPERATIONS_ZH.md) to transfer the IMG and any missing J7 files by `net-scp`. Matching FVR17 J7 files can stay on the SD card. Then run `boot FVR18.IMG --name FVR18.IMG` and `reset`. For a manual microSD update, back up `config.txt`, copy the IMG to the boot partition root, put missing J7 files in `XDOS/INBOX`, and set `kernel=FVR18.IMG`.
3. Read the four percentages on the bottom taskbar. Start an XAPP from the desktop, or use UART `OPEN NOTEPAD`, and close it normally. Names clear after the corresponding apps close.
4. To roll back, select `FVR17.IMG` with BOOT and RESET, or restore its kernel entry in `config.txt`. Both releases use the same J7 files.

## Pi 3B+ hardware record

FVR18 booted at 1920×1080 after network deployment and CRC32 verification. UART confirmed an idle desktop with all four V2 slots free, then Notepad and a background Core Demo running together with CPU3 borrowed. After closing both, all slots were free and CPU3 returned to LVGL. HDMI acceptance confirmed that all four percentages and the active app names displayed correctly at idle, during execution and after exit.
