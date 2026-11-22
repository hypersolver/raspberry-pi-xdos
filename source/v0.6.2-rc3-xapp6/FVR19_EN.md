# FVR19 for Raspberry Pi 3B+

FVR19 adds boot modes to FVR18 and keeps the same ten J7 XAPPs. Verify the downloads against `SHA256SUMS.txt`. Existing matching J7 files may stay on the card.

## Install and configure

Follow the [deployment guide](OPERATIONS_ZH.md) for microSD installation or network transfer. To deploy over the network, transfer `FVR19.IMG`, select it with `boot FVR19.IMG --name FVR19.IMG`, and send `reset`. For manual installation, copy the image to the boot volume root and set `kernel=FVR19.IMG` in `config.txt`.

Set the boot mode in `XDOS/XDOS.CFG` on the microSD card. XDOS reads `XDOS/INBOX/XDOS.CFG` if the primary file is absent. Back up both locations before testing.

- `MODE=AUTO` selects Desktop when `KIOSK` is empty and Kiosk when it is set. Headless detection is not part of this build. Set `MODE=IOT` for headless operation.
- `MODE=DESKTOP` opens the desktop.
- `MODE=KIOSK` launches `KIOSK=<file.XAPP>` full screen. `KIOSK_EXIT=RESTART|DESKTOP|REBOOT` controls what happens when that app ends.
- `MODE=IOT` suspends LVGL refresh and accepts only `Screen=None` XAPPs.
- `AUTORUN=<file1.XAPP>,<file2.XAPP>` starts background apps in any mode.

Use UART `@XDOS MODE` to query the current mode. `@XDOS MODE DESKTOP` and `@XDOS MODE KIOSK` switch between those modes at runtime. Change the microSD configuration and reset to enter or leave IOT.

To roll back, restore the original configuration files, select `FVR18.IMG` and reset. The paired J7 XAPPs can stay on the card.

## Troubleshooting

- If a setting seems ignored, check the primary file first. It takes priority over the INBOX copy.
- If BOOT rejects an image, verify the downloaded checksum and check whether the card already contains an image with the same name.
- If a Kiosk app fails to open three times, XDOS returns to Desktop. Verify the filename and that the module supports full-screen operation.

## Hardware regression

On a Raspberry Pi 3B+ at 1920×1080, network transfer passed CRC32 and UART verified boot and runtime mode selection, desktop and Kiosk/IOT AUTORUN, all three Kiosk exit policies, and IOT rejection of windowed apps. The test removed both temporary configuration paths and verified a return to FVR18 with UART READY. HDMI visual inspection was outside this run.
