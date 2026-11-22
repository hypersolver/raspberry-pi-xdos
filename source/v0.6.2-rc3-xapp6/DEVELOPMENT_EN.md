# XDOS development and validation

PXLAYOUT is the public prerelease. Raspberry Pi 3B+ testing passed at 720p and 1080p, including icon and label clicks for all nine desktop shortcuts. PXBOOT and the 128-row LVGL build were retained as rollback baselines. See the [release guide](PXLAYOUT_EN.md).

## Experimental media builds

| Work | Test result and remaining issue |
| --- | --- |
| 720p H.264 playback | All 359 frames of the 30 FPS test clip were presented. P95 frame interval was about 85 ms and the maximum was about 166 ms. The EGL swap call had a P95 of about 2 ms. The inherited all-intra timestamp FIFO assigned timestamps in input order to B-frame output, producing 166 non-increasing presentation timestamps. A scheduling fix remains to be validated. |
| 1080p H.264 playback | The I420 buffer was aligned to 1920×1088 and the visible image cropped to 1920×1080. VideoCore reported a resource error in both trials before the first frame was displayed. |
| Music XAPP | A 44.1 kHz MP3 played to completion over HDMI and the 3.5 mm jack. The prototype also supports 16-bit PCM WAV at 44.1 kHz. It decodes MP3 on ARM. Recording has not been implemented and requires an external input device. |
| 3.5 mm stereo investigation | Separate left and right signals were audible over HDMI. UART showed both PWM channels enabled and distinct signals in both DMA FIFO positions. The physical cause of the single-ear analogue output was not established. |

Experimental images and commands are distinct from the public PXLAYOUT release. OPEN MUSIC and the AUDIO probe are available only on the corresponding development images.

## Core image and optimization candidates

The released kernel is 1,491,628 bytes. Its .text is 1,240,776 bytes and its .rodata is 249,288 bytes. The 157,108-byte .bss and the 1 MiB XAPP address-space reservation are allocated in RAM. The XAPP execution slot within that reservation is 320 KiB. Debug sections are available in the ELF for diagnosis and excluded from the IMG.

1. Remove unused MP4 muxing functions from the playback build and check that demuxing still works.
2. Audit LVGL pixel formats and font imports. The six linked Montserrat font sizes account for about 108 KiB, and external demos use linked resources.
3. Check whether OpenVG can be removed from the shared EGL/GLES2 client while keeping hardware VSync and VSyncOpt fallback.
4. Profile runtime RAM separately. A 1920×1080 RGB565 canvas takes about 3.96 MiB, and two 128-row partial refresh buffers take about 0.94 MiB.

The sizes describe the current build. Savings require a rebuilt image and regression testing with its matching XAPPs.

## FILECLR

Pi 3B+ boot, Files launch over UART, and the HDMI color fix were verified. See [changes, installation and test results](FILECLR_EN.md).
