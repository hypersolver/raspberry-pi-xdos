# FVMEDIA for Raspberry Pi 3B+

Browser, MP4 Player, MP3 Player, official LVGL Widgets and official Benchmark now run as external V2 XAPPs. Files, Notepad, Calculator, Paint and Terminal were rebuilt for the same kernel. The core controls window stacking, keyboard focus and drag limits. Benchmark retains its full assets and uses the 768 KiB slot alongside three 320 KiB slots.

## Installation and operation

1. Download `FVMEDIA.IMG`, all ten `V7*.XAPP` files and `SHA256SUMS.txt`. Verify the binaries with `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash -Algorithm SHA256`.
2. For manual microSD update, back up `config.txt`, place the IMG in the root of the working XDOS FAT32 card, place the XAPPs in `XDOS/INBOX`, and set `kernel=FVMEDIA.IMG`. Keep the existing shortcut manifests.
3. For network update, run `net-scp` for each XAPP and the IMG at UART 921600 baud, 8N1. Then run `boot FVMEDIA.IMG --name FVMEDIA.IMG` and `reset`. See the [deployment guide](OPERATIONS_ZH.md).
4. Click a visible part of a window to raise it and focus the keyboard. Browser uses HTTP and supports OPEN and STOP. Choose an MP4 from SD or use UART `PLAY <file>`. MP3 Player accepts MP3 and 44.1 kHz PCM WAV and offers HDMI and 3.5 mm output. Use BACK TO DESKTOP to leave Widgets or Benchmark.
5. If an app fails to start, check the paired V7 file and checksum. If the large slot is occupied, close the app occupying slot 3 before launching Benchmark. To roll back, select FVAPPS, FVWIND or FILESV2 with BOOT and RESET, or restore the previous kernel in `config.txt`. Keep its matching FA, FW or FV files.

## Hardware verification

On Pi 3B+ at 1920×1080, the IMG and ten XAPPs passed network CRC32 and the board booted after RESET. UART confirmed all five new V2 apps started. Widgets and Benchmark each released slot 3 on exit while Browser and both players remained active. User acceptance confirmed foreground stacking, keyboard focus and operation of all five apps.
