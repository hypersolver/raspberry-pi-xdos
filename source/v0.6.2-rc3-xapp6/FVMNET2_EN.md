# XDOS FVMNET2 for Raspberry Pi 3B+

FVMNET2 adds adaptive wired-network polling to FVMEDIA. The network task polls at 1 ms for 20 ms after traffic and at 10 ms when idle. During FETCH downloads it keeps the 1 ms interval to sustain transfer, then resumes adaptive polling. The FVMEDIA desktop and application set are retained. This build requires its matching ten N9 XAPPs.

## Install, operate and roll back

1. Download `FVMNET2.IMG`, all ten `N9*.XAPP` files and `SHA256SUMS.txt`. Verify with `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash -Algorithm SHA256`.
2. For a network update, use the XDOS UART tool at 921600 baud, 8N1. Run `net-scp` for each XAPP and the IMG using their published names. Then run `boot FVMNET2.IMG --name FVMNET2.IMG` and `reset`. See the [deployment guide](OPERATIONS_ZH.md).
3. For a manual update, back up `config.txt` on a working XDOS FAT32 card. Copy the IMG to the root and the N9 XAPPs to `XDOS/INBOX`, set `kernel=FVMNET2.IMG` and boot. Keep existing desktop shortcuts.
4. Use the desktop apps as described in the [FVMEDIA guide](FVMEDIA_EN.md). UART `[PROF]` shows CPU0 busy time and `[NET]` reports polling activity. If an app fails to open, check its matching N9 file and checksum.
5. To roll back, select `FVMEDIA.IMG` with BOOT and RESET. If UART is unavailable, reselect the FVMEDIA image in `config.txt`. Keep its matching V7 XAPPs.

## Pi 3B+ verification

Across five approximately 5-second desktop idle samples, CPU0 busy went from 12.1% on FVMEDIA to 2.0% on FVMNET2. FVMNET2 recorded 117–121 network polls per second at idle.

| Wired FETCH, same 1,592,512-byte payload | Time | End-to-end rate |
| --- | ---: | ---: |
| FVMEDIA baseline | 3.824 s | 406.7 KiB/s |
| FVMNET2 run 1 | 5.780 s | 269.1 KiB/s |
| FVMNET2 run 2 | 3.861 s | 402.8 KiB/s |
| FVMNET2 run 3 | 3.707 s | 419.5 KiB/s |

The initial integration failed two FETCH attempts. With the 1 ms FETCH interval, all three timed runs passed CRC32. The first was slower than baseline, and the next two were close to baseline. The core and all ten modules passed deployment CRC32. FVMNET2 confirmed BOOT, acknowledged RESET and returned UART PING READY after reboot. V2 Files opened successfully.
