# Raspberry Pi XDOS

[繁體中文](README_ZH.md) · [日本語](README_JA.md) · [FVR21 prerelease](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-fvr21) · [Base XAPP6 Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6)

XDOS is an ARM32 bare-metal desktop for Raspberry Pi. Circle provides the hardware support, LVGL draws the desktop, and the system loads external XAPPs from microSD. The earlier PXLAYOUT prerelease was tested on a Raspberry Pi 3B+ at 1280×720 and 1920×1080.

## RPiDuino Arduino SDK

The Arduino SDK, board package, and examples are maintained in [RPiDuino](https://github.com/hypersolver/RPiDuino). [Arduino IDE setup](https://github.com/hypersolver/RPiDuino/blob/main/README.en.md) · [Boards Manager index](https://raw.githubusercontent.com/hypersolver/RPiDuino/main/package_rpiduino_index.json) · [Legacy XDOS index](https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json)

## Quick start

- [SDPRE release, installation and rollback guide](docs/sdpre.md)

- [MultiCore Media release guide and troubleshooting](docs/multicore-media.md)

- [MultiCore APPSCAN release guide and troubleshooting](docs/multicore-appscan.md)

1. [Prepare the microSD and start the XDOS desktop (繁體中文)](docs/xdos-quick-start.zh-TW.md)
2. [Build, deploy and run Arduino sketches as XAPPs (繁體中文)](docs/arduino-xapp.zh-TW.md)
3. [XAPP desktop shortcut and icon specification](docs/xapp-desktop-spec.md)

## FVR19 hardware screenshots

FVR19 running on a Raspberry Pi 3B+, captured over HDMI at 1920×1080. The desktop LAN address is masked.

| Boot splash | Desktop |
| --- | --- |
| ![FVR19 boot screen](docs/fvr19/boot.png) | ![FVR19 desktop](docs/fvr19/desktop.png) |

[LVGL Benchmark results screenshot](docs/fvr19/benchmark.png)

## Why XDOS

XDOS started as a port of XD OS to Raspberry Pi. It explores how far a small bare-metal core can provide an interactive desktop, with applications developed and replaced independently. The project also serves as a test bed for boot timing, partial redraw and hardware-assisted display on the Pi 3B+.

## System architecture

[Current development record, multicore, boot modes, Arduino, and flashing diagrams](docs/architecture.md)

~~~mermaid
flowchart TD
    Boot["Pi boot firmware and config.txt"] --> Kernel["XDOS ARM32 kernel"]
    Kernel --> Circle["Circle hardware services"]
    Circle --> Devices["SD / FAT32, USB, Ethernet and UART"]
    Kernel --> UI["LVGL desktop and application windows"]
    UI --> Display["VC4 GLES2 / EGL VSync or framebuffer fallback"]
    Kernel --> Loader["XAPP loader and ARM32 host API"]
    Loader --> Apps["External XAPPs on microSD"]
    Apps --> UI
~~~

The core owns startup, device services, the desktop, the four-slot V2 runtime and recovery commands. V2 modules can bind to compatible host exports at load time. Bound modules still require the matching kernel fingerprint.

## Development and deployment

~~~mermaid
flowchart LR
    Source["Source and pinned dependencies"] --> Build["WSL2 ARM32 cross-build"]
    Build --> Bundle["Kernel IMG, paired XAPPs and SHA-256"]
    Bundle --> Stage["UART commands and LAN file transfer"]
    Stage --> Pi["Pi 3B+ hardware validation"]
    Pi --> Release["GitHub main, release tag and assets"]
    Pi -->|Failure| Rollback["Select previous IMG and reset"]
~~~

Review IMG and ELF changes, build matching XAPPs, and confirm startup, display, input and rollback on hardware before publishing. Each release keeps its own checksums and validation notes.

## Releases

| Release | When to use it |
| --- | --- |
| [Base RC3 XAPP6](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6) | Start with the ARM32 baseline, external LVGL demos and base test package |
| [Pixel BBS Layout](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-pxlayout) | Earlier desktop release with adaptive desktop layout and an SD-based H.264 player |
| [MultiCore Pi 3B+](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-multicore) | Evaluate four-core LVGL rendering and 1080p benchmark improvements. [Features and setup](source/v0.6.2-rc3-xapp6/MULTICORE_EN.md) |
| [All releases](https://github.com/hypersolver/raspberry-pi-xdos/releases) | Find earlier builds, assets and rollback versions |

## Documentation

| What you want to do | Guide |
| --- | --- |
| Install or update PXLAYOUT and use the desktop | [Installation and operations](source/v0.6.2-rc3-xapp6/PXLAYOUT_EN.md) |
| Build from source | [Build and dependency revisions](source/v0.6.2-rc3-xapp6/PXLAYOUT_SOURCE.md) |
| Use UART commands or develop an XAPP | [UART and XAPP API reference](source/v0.6.2-rc3-xapp6/API_EN.md) |
| Check the hardware results, experimental features and core size | [Development and validation](source/v0.6.2-rc3-xapp6/DEVELOPMENT_EN.md) |
| Find earlier releases and recovered source | [Version index](VERSIONS.md) |

The PXLAYOUT core image is 1,491,628 bytes. Download the matching XAPPs and SHA-256 manifest from the same Release.

## Future applications

The architecture can support embedded status panels, offline utility desktops and bare-metal teaching or test benches. Optional media modules and more device drivers are candidates for future work. MP3 playback is available in the media releases. 1080p decoding and audio capture still have documented limitations.

Documentation and source are maintained on main. Release tags identify published builds.

**MC4FQ for Pi 3B+** · Four concurrent V2 XAPPs · [Release and FILEQ128 upgrade instructions](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-mc4fq)

**FILEQ128 and RPiDuino 0.1.4 preview** · [Setup, changes and validation](docs/fileq128.md) · [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-fileq128)

[Source and license manifest](SOURCE_MANIFEST.md)
