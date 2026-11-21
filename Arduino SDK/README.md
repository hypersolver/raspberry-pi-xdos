# RPiDuino Arduino SDK 0.1.2

[繁體中文](README.zh-TW.md)

RPiDuino builds standalone Arduino sketches for Raspberry Pi using Circle. The first version provides `setup()`, `loop()`, digital GPIO, the ACT LED, and timing functions. It exports a raw kernel IMG for a Raspberry Pi boot microSD. This package targets standalone firmware. XDOS and XAPPs use their own build flow.

## Install

1. Install Python 3 and the build dependencies below. On Windows, install Python on Windows and the toolchain in the default WSL Ubuntu distribution.
2. In Arduino IDE, open **File → Preferences → Additional Boards Manager URLs** and add the URL below.
3. Open Boards Manager, search **RPiDuino**, and install **0.1.2**. Select the board under **Tools → Board → RPiDuino**. Open Blink under **File → Examples → RPiDuino Basics**.

    https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json

From Ubuntu or WSL:

```sh
sudo apt install git make python3 gcc-arm-none-eabi libstdc++-arm-none-eabi-dev gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json
arduino-cli core install rpiduino:bcm@0.1.2 --additional-urls https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json
arduino-cli board listall RPiDuino
arduino-cli compile --fqbn rpiduino:bcm:rpiduino3 --export-binaries /path/to/Blink
```

On the first build, the SDK fetches a pinned Circle revision into the Linux user cache. Set `RPIDUINO_CIRCLE_SOURCE` in the Linux build environment to use a local repository containing that revision.

## Raspberry Pi 3/3B+

The Micro-USB connector supplies power. Use Debug UART, microSD, or the [XDOS Ethernet transfer workflow](../source/v0.6.2-rc3-xapp6/NETWORK_UPDATE_ZH.md), which triggers a network fetch over UART.

### Debug UART

Boot a dedicated test microSD into an XDOS build with its UART updater enabled at 921600 baud. Keep your original XDOS card separate. The updater needs a single active `kernel=` entry in `config.txt` and a 32-bit configuration. Install pyserial in the Python environment used by the IDE (`python -m pip install pyserial` on Windows).

Connect a 3.3 V USB–TTL adapter to the Pi. Adapter RX goes to physical pin 8, TX to pin 10, and GND to pin 6. Leave VCC disconnected. Select **RPiDuino 3/3B+** and the adapter's COM port, close Serial Monitor, and click **Upload**. The tool uses the existing XDOS `PUT` / `DATA` / `END` → `BOOT` → `RESET` workflow with CRC32. The new IMG gets a distinct filename. The original kernel file stays on the test card.

Standalone Blink leaves no XDOS command service running. To upload again, power off, restore the test card's `xdosboot.bak` as `config.txt` (keep a copy of the backup), and boot XDOS again. Blink's ACT LED should alternate 500 ms on and 500 ms off.

## Raspberry Pi 4B

Select **RPiDuino 4** for USB-C CDC upload. For a manual 64-bit microSD boot use `kernel8-rpi4.img`.

## Raspberry Pi 5

Select **RPiDuino 5** for USB-C CDC upload. For a manual 64-bit microSD boot use `kernel_2712.img`.

## Pi 4B and 5 USB-C CDC upload

Use a dedicated test microSD with 64-bit Raspberry Pi OS. Install `python3-serial` and copy `tools/cdc_receiver.py` from the board package to the Pi. Under `[all]` in `/boot/firmware/config.txt`, add `dtoverlay=dwc2,dr_mode=peripheral`, ensure `otg_mode=1` is disabled, and reboot.

```sh
sudo touch /boot/firmware/RPIDUINO.TEST
sudo modprobe g_serial
sudo python3 cdc_receiver.py
```

After ensuring adequate power, connect the Pi's USB-C data port to Windows with a data cable. Install `pyserial` in the IDE's Python environment, select the board and its COM port, then click **Upload**. The uploader checks the target board and the receiver checks CRC32 before saving the IMG and rebooting. To return to the receiver, power off, restore the test card's `rpiduino-config.bak` as `config.txt`, move the backup off the boot partition, then boot and restart the receiver. Physical USB enumeration and boot still need validation. [Raspberry Pi OTG setup and power](https://pip-assets.raspberrypi.com/categories/685-app-notes-guides-whitepapers/documents/RP-009276-WP-1-Using%20OTG%20mode%20on%20Raspberry%20Pi%20SBCs)

## Boot from microSD

Use **Sketch → Export Compiled Binary** in Arduino IDE. Copy the exported IMG to a separate FAT boot microSD with Raspberry Pi boot firmware for your model. Rename it as shown and add the two settings to `config.txt`.

| Board | FQBN | IMG and kernel setting | Architecture setting |
| --- | --- | --- | --- |
| RPiDuino 2 | `rpiduino:bcm:rpiduino2` | `kernel=kernel7.img` | `arm_64bit=0` |
| RPiDuino 3/3B+ | `rpiduino:bcm:rpiduino3` | `kernel=kernel8-32.img` | `arm_64bit=0` |
| RPiDuino 4 | `rpiduino:bcm:rpiduino4` | `kernel=kernel8-rpi4.img` | `arm_64bit=1` |
| RPiDuino 5 | `rpiduino:bcm:rpiduino5` | `kernel=kernel_2712.img` | `arm_64bit=1` |

GPIO uses BCM numbers 0–27. `LED_BUILTIN` selects the board's ACT LED. Build verification covers all four profiles using Arduino CLI and WSL. Physical boot and Windows Arduino IDE builds still need validation.

For local development, copy `hardware/rpiduino` into the `hardware` directory in your Arduino sketchbook. To rebuild the versioned archive and index from the repository root, run `python "Arduino SDK/package.py"`. For new releases, update the version in `platform.txt`, generate and commit the archive and index, and retain previous archives. [Arduino platform specification](https://docs.arduino.cc/arduino-cli/platform-specification/) · [Circle and Raspberry Pi boot setup](https://github.com/rsta2/circle#installation)
