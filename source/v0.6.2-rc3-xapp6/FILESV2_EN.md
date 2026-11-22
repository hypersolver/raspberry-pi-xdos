# FILESV2 for Raspberry Pi 3B+

Files now runs as the external V2 app FVFILES.XAPP. It opens from the desktop or UART and can run alongside Notepad. The core still enforces the directory page size and validates single-file deletion requests.

## Update an existing XDOS card

1. Download FILESV2.IMG, FVFILES.XAPP, FVNOTE.XAPP and SHA256SUMS.txt. Verify the three binaries using `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash -Algorithm SHA256`.
2. Copy the IMG to the root of the FAT32 microSD card and both XAPPs to XDOS/INBOX. Back up config.txt, set `kernel=FILESV2.IMG` and boot the Pi. Leave the existing desktop shortcut manifests in place.
3. For a network update, use the UART transfer tool at 921600 baud, 8N1. Run `net-scp` for the two XAPPs and the IMG using their exact filenames. Then run `boot FILESV2.IMG --name FILESV2.IMG` and `reset`. See the [base installation and deployment guide](OPERATIONS_ZH.md) for setup.
4. To roll back, select FILECLR.IMG with BOOT and reset, or restore the previous microSD config.txt. Keep its matching CLNOTE.XAPP in XDOS/INBOX.

## Use Files

- Single-click a file to select it. The list is white and selected files use a blue background with white text.
- Double-click an XAPP to launch it. Click a folder to navigate. Use the wheel or scrollbar to browse, and Prev or Next if the directory exceeds the 128-entry page limit.
- Hold a file row and press Delete to confirm deletion of that one file. Directories cannot be deleted by this action.
- From UART, use `OPEN FILES` and `OPEN NOTEPAD`. If either fails to start, verify the paired XAPP is in XDOS/INBOX and matches the manifest.

## Pi 3B+ test record

- The wired deployment passed CRC32 for the IMG and both XAPPs. RESET booted FILESV2 at 1920×1080.
- UART confirmed Files in V2 slot 0 and Notepad in slot 1. After Files closed, slot 0 was free and Notepad remained active. Reopening Files restored both active slots.
- HDMI verification passed for the white background, blue selection with white text, double-click launch, wheel scrolling and single-file deletion.

One initial launch logged an LVGL style initialization warning. Source review found a candidate in LVGL transition-style allocation: it passes uninitialized storage to a check that reads the previous style sentinel. An instrumented Pi 3B+ reboot did not reproduce the warning, so the original call site remains unconfirmed. The validated firmware was restored and both apps opened again.
