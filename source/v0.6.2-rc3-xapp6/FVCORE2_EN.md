# FVCORE2 for Raspberry Pi 3B+

FVCORE2 adds the R16-B scheduling policy on top of FVMNET2. The desktop parses `Screen=Full|Window|None`. Legacy V1 modules are fullscreen, and V2 manifests default to Window. A second fullscreen XAPP is rejected with error `-11`. The runtime lends up to three cores in windowed or headless mode and up to two while a fullscreen app is active, leaving a core for LVGL. After three consecutive CPU0 work slices exceed 10 ms, the runtime runs that job once every four ticks and reports the slowdown. Borrowed-core apps can use `xdos_core_time_us()` and `xdos_core_sleep_us()`.

## Install, test and roll back

This is a Pi 3B+ test release. It includes the tested IMG, paired Files and Notepad, and the Core Demo, throttle and sleep test XAPPs. The other FVMNET2 desktop apps are outside the tested package. Keep FVMNET2 and its N9 modules for a full desktop rollback.

1. Download `FVCORE2.IMG`, the five FK XAPPs and `SHA256SUMS.txt`. Verify with `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash -Algorithm SHA256`.
2. With the [deployment guide](OPERATIONS_ZH.md), use UART at 921600 baud, 8N1 and run `net-scp` for the IMG and all five XAPPs under their published names. Run `boot FVCORE2.IMG --name FVCORE2.IMG`, then `reset`. For manual update, back up `config.txt`, copy the IMG to the FAT32 card root, put the five XAPPs in `XDOS/INBOX`, and select the IMG in `config.txt`.
3. UART accepts `XAPP2 FKCORE.XAPP NONE`, `XAPP2 FKNOTE.XAPP FULL`, `XAPP2 FKSLOW.XAPP`, and `XAPP2 FKSLEEP.XAPP`. Use `XAPP2-STATUS` to see slots and borrowed cores, and `XAPP2-CLOSE <slot>` to close a test.
4. To roll back, select `FVMNET2.IMG` with BOOT, then RESET. Verify the FVMNET2 boot banner and PING READY. If UART is unavailable, restore the prior kernel selection in `config.txt`.

## R16-B hardware record

On Pi 3B+, all uploads passed network CRC32 and FVCORE2 booted after RESET. The test job returned exit 0.

| Test | Result |
| --- | --- |
| Four headless apps | Slots 0–3 active, three borrowed cores and the fourth app sharing CPU0. All slots released on close. |
| Fullscreen exclusivity | One fullscreen and three windowed apps ran together. Two cores were borrowed and CPU1 remained with LVGL. A second fullscreen request returned `-11`. |
| CPU0 throttle | Work slices of about 15 ms triggered the slowdown notice and `app/throttled` status. |
| Borrowed-core sleep | CPU3 completed 20 × 100 ms in 2,001 ms. Maximum per-sleep overshoot was 125 µs. |
| Early stop | CLOSE interrupted the second sleep job before completion and released its slot. |
| Rollback | BOOT and RESET restored FVMNET2. The boot banner and UART PING READY were verified and COM3 was released. |

Arduino V2 delay and generation of the `Screen=` manifest field remain separate tasks.
