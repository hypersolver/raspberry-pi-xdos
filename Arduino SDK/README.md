# RPiDuino Arduino SDK 0.1.0

[繁體中文](README.zh-TW.md)

RPiDuino builds standalone Arduino sketches for Raspberry Pi using Circle. The first version provides `setup()`, `loop()`, digital GPIO, the ACT LED, and timing functions. It exports a raw kernel IMG for a Raspberry Pi boot microSD. This package targets standalone firmware. XDOS and XAPPs use their own build flow.

## Install

1. Install Python 3 and the build dependencies below. On Windows, install Python on Windows and the toolchain in the default WSL Ubuntu distribution.
2. In Arduino IDE, open **File → Preferences → Additional Boards Manager URLs** and add the URL below.
3. Open Boards Manager, search **RPiDuino**, and install **0.1.0**. Select the board under **Tools → Board → RPiDuino**. Open Blink under **File → Examples → RPiDuino Basics**.

    https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json

From Ubuntu or WSL:

```sh
sudo apt install git make python3 gcc-arm-none-eabi libstdc++-arm-none-eabi-dev gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json
arduino-cli core install rpiduino:bcm@0.1.0 --additional-urls https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json
arduino-cli board listall RPiDuino
arduino-cli compile --fqbn rpiduino:bcm:rpiduino3 --export-binaries /path/to/Blink
```

On the first build, the SDK fetches a pinned Circle revision into the Linux user cache. Set `RPIDUINO_CIRCLE_SOURCE` in the Linux build environment to use a local repository containing that revision.

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
