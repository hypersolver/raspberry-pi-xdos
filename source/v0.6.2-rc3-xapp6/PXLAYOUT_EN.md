# Pixel BBS Layout

[繁體中文](PXLAYOUT_ZH.md) · [Release downloads](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-pxlayout)

This ARM32 prerelease was tested on a Raspberry Pi 3B+. The boot splash and desktop share a procedural Pixel BBS theme. The boot bar tracks completed startup stages. Desktop icons, text, spacing and click targets adapt to the display size.

| Hardware test | Icon size / row spacing | Icons opened the correct XAPP | Names opened the correct XAPP |
| --- | --- | ---: | ---: |
| 1280×720 | 31×31 / 61 px | 9/9 | 9/9 |
| 1920×1080 | 47×47 / 100 px | 9/9 | 9/9 |

The nine shortcuts are Terminal, Notepad, Calculator, Paint, Files, Browser, LVGL Demo, the official LVGL Benchmark and MP4 Player. UART confirmed that XDOS, LVGL, VC4 and HDMI reported the same dimensions in both runs.

## Download and verify

Download PXLAYOUT.IMG and the five matching modules, PLAYER.XAPP, Q9WIDGET.XAPP, Q9BENCH.XAPP, Q9BROWSE.XAPP and Q9TERM.XAPP. In Linux or WSL, run sha256sum -c SHA256SUMS.txt in the download directory. In PowerShell, use Get-FileHash -Algorithm SHA256 and compare against the manifest. Source is available as PXLAYOUT-source.tar.gz. See [build instructions](PXLAYOUT_SOURCE.md).

## Install on microSD

1. Prepare a FAT32 boot partition with compatible Raspberry Pi boot firmware and the XDOS base files. The [base test package](packages/XDOS_RC3_XAPP6_PI3_TEST.zip) supplies XDOS application files. Keep a copy of the previous config.txt, kernel and matching XAPPs.
2. Copy PXLAYOUT.IMG to the boot partition root. This file is the bare-metal kernel. In config.txt, set arm_64bit=0 and kernel=PXLAYOUT.IMG in the applicable section. Keep the working firmware, HDMI and UART settings.
3. Copy the five paired XAPPs under XDOS/INBOX using their Release names. Keep the base applications and manifests under XDOS/APPS, and installed device drivers under XDOS/DRIVERS.
4. Safely eject the card and boot the Pi. The serial console is 921600 baud, 8N1.

## Update over Ethernet

Connect the Pi and host to the same LAN and attach a 3.3 V USB–TTL adapter. Install pyserial on the host. Run the commands from the version directory, replacing SERIAL_PORT, HOST_LAN_IP and file paths with your own values.

~~~text
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 ping
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp PLAYER.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9WIDGET.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9BENCH.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9BROWSE.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9TERM.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp PXLAYOUT.IMG --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 boot PXLAYOUT.IMG --name PXLAYOUT.IMG
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 reset
~~~

Each transfer checks length and CRC32. FETCH rejects an existing destination name. Skip an already staged file only after confirming it matches this Release. The default local HTTP port is 8765. When rebooted, check PING and the DISPLAY line over UART.

## Use the desktop and roll back

Click a shortcut icon or its name. For local H.264 video, open MP4 PLAYER, choose an SD file and press PLAY. Over UART, send PLAY SD:/XDOS/INBOX/DEMO.MP4. CLOSE stops playback. This release requires a video width divisible by 32 and height divisible by 16. HDMI desktop output was tested at 1080p. Video at 1920×1080 is outside this player's supported dimensions. The released player provides video only.

To return to the immediately previous boot configuration, send BOOT-CLEAR followed by RESET. To select a specific retained release such as PXBOOT, use BOOT with its matching local IMG for CRC32 verification, followed by RESET. If UART is unavailable, shut down the Pi, mount the card on a computer, and restore config.txt or point kernel= to the retained IMG.
