# XDOS MultiCore for Raspberry Pi 3B+

[繁體中文](MULTICORE_ZH.md) · [Release assets](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-multicore)

MultiCore is a wired ARM32 performance prerelease derived from the PERF7 VC4 AnalyticsRegionGuard line. PXLAYOUT remains the separate Pixel BBS desktop and media release. Keep each kernel with its matching XAPPs.

## What changed

- Core 0 runs the desktop, USB, networking, UART and VideoCore services. Cores 1–3 run LVGL software drawing workers. Draw tasks are assigned by object and the tile count stays at one.
- VC4 uses an opaque RGB565 EGL surface and checks GL errors once per frame. It retains EGL VSync and removes the redundant finish before swapping.
- LVGL uses a 1 ms timebase and a 16 ms redraw period. During active XAPPs, the core yields to other services and uses WFE with the ARM timer event stream.
- The official Benchmark scrolling scene reuses GPU texture content up to 1920×1080 and fences the GPU shift before uploading exposed bands. Widgets Analytics keeps the RegionGuard path.
- The desktop CPU indicator reports core 0. The build uses Ethernet, and has no built-in Wi-Fi.

## Hardware results

The supplied Pi 3B+ UART benchmark exports use 1920×1080 at 60 Hz for both HDBENCH and MultiCore. These results come from the release package's recorded runs.

| Metric | HDBENCH | MultiCore |
| --- | ---: | ---: |
| LVGL Benchmark reported average FPS | 17 | 46 |
| Average render time | 23 ms | 11 ms |
| Average flush time | 44 ms | 8 ms |
| Containers with scrolling | 10 FPS | 44 FPS |
| Containers with overlay | 8 FPS | 19 FPS |
| Screen sized text | 10 FPS | 19 FPS |

The package contains both raw CSV exports. The release image is 1,448,296 bytes and was built with test debug output disabled. Visual scrolling and input checks were recorded using TuneC and TuneD builds from the same code line.

## Install and update

1. Verify downloaded assets with sha256sum -c SHA256SUMS.txt from their directory. Preserve the previous config.txt, kernel and paired XAPPs.
2. For a prepared FAT32 Raspberry Pi boot partition, extract XDOS-MultiCore-Pi3B-Plus-SD.zip to its root. Keep the Raspberry Pi firmware files and set arm_64bit=0 and kernel=XDOSMC.IMG in the applicable config.txt section.
3. To deploy over Ethernet from an existing XDOS install, use net-scp to stage LVGLDEMO.XAPP as MCWIDGET.XAPP, LVGLBENCH.XAPP as MCBENCH.XAPP, BROWSER.XAPP as MCBROWSE.XAPP, and TERMINAL.XAPP as MCTERM.XAPP. Stage XDOSMC.IMG and wait for CRC32 verification on each transfer. Run boot XDOSMC.IMG --name XDOSMC.IMG, then reset. The [UART guide](UART_ZH.md) and [English API reference](API_EN.md) cover the transport syntax.
4. This image accepts raw UART HDMI-MODE 720P or HDMI-MODE 1080P, prefixed with @XDOS and a space. Reset after selecting a mode. Use BENCH-RESULT to read a completed official benchmark.
5. Roll back using boot-clear followed by reset while UART is available. Otherwise restore the backed-up config.txt or select the previous kernel on the SD card.

The source archive includes the build tools, Arduino sketches and the publicly sanitized RC3 base archive. The exact configuration command and dependency revisions are in the [Traditional Chinese notes](MULTICORE_ZH.md#從原始碼建置).
