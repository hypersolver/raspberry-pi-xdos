# XDOS architecture and development record

[繁體中文](architecture.zh-TW.md) · [日本語](architecture.ja.md) · [Version index](../VERSIONS.md)

## Published milestones and experimental work

| Milestone | What was verified |
| --- | --- |
| FVR17 | External V2 imports bind across compatible kernels. Identical uploads succeed and conflicting contents under the same name are rejected. |
| FVR18 | Four CPU utilization values and active XAPP names were checked on Pi 3B+. |
| FVR19 | 44 UART checks passed for boot modes, AUTORUN, Kiosk exit and rollback. |
| FVR20 | Host 921600 baud, 400 MHz mini UART clock, UART and Ethernet CRC32, and long directory listings passed. |
| FVR21 | Read-only UART NET reports live IPv4, mask, gateway and address mode. DHCP and host interface selection passed. Static and disconnected cases were not tested on hardware. |

The FVR19 1080p LVGL baseline measured 41 FPS overall and 43 FPS for scrolling containers. Full-screen text and container overlays remain optimization targets. Experimental work on sparse redraw, font assets, image decoding and broader V2 migration has separate test builds. These changes are pending individual integration into published releases.

## Architecture and deployment diagrams

CPU0 owns hardware I/O and the UI event loop. CPUs 1–3 can process LVGL drawing or be lent to eligible XAPP compute jobs. Up to four V2 slots may be active, with a shared CPU0 path when eligible. Device and UI service calls must follow the host API and its core-affinity rules.

The public documentation is available in English, Traditional Chinese and Japanese. The test builds load Traditional Chinese LVGL font assets from microSD with separate license files. FVLANG1 additionally prototypes SD-based language dictionaries for desktop labels. The Japanese guide describes the current system. A Japanese runtime pack has not been validated.

On FVR21, AUTO follows the KIOSK setting without HDMI detection. AUTORUN launches background XAPPs across modes. IOT suspends LVGL refresh and accepts Screen=None. Kiosk runs one fullscreen XAPP and applies the chosen exit policy.

For Arduino development, use the [RPiDuino SDK](https://github.com/hypersolver/RPiDuino). Select the Pi 3/3B+ XDOS XAPP target and deploy the compiled module to an existing XDOS installation. The kernel IMG is the bare-metal executable copied onto the FAT boot partition alongside Pi firmware. Use the [operations guide](../source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) for SD preparation and rollback.

### System overview

~~~mermaid
flowchart TD
    FW["Pi firmware + config.txt"] --> IMG["XDOS ARM32 IMG"]
    IMG --> C["Circle: FAT32 / USB / Ethernet / UART"]
    IMG --> D["XDOS desktop + LVGL"]
    D --> VC["VC4 GLES2 / EGL VSync"]
    D --> FB["VSyncOpt / framebuffer"]
    IMG --> V2["XAPP V2 loader"]
    V2 --> APPS["microSD: .APP + .XAPP"]
~~~

### Multicore scheduling

~~~mermaid
flowchart LR
    CPU0["CPU0: UI / IRQ / I/O / scheduler"] --> DRAW["LVGL draw work"]
    DRAW --> C1["CPU1"]
    DRAW --> C2["CPU2"]
    DRAW --> C3["CPU3"]
    C1 -. "lend when available" .-> APP["XAPP compute"]
    C2 -.-> APP
    C3 -.-> APP
    APP -->|request| CPU0
~~~

### Desktop, Kiosk and IOT modes

~~~mermaid
flowchart TD
    CFG["XDOS/XDOS.CFG or INBOX fallback"] --> MODE{"MODE"}
    MODE -->|DESKTOP| D["Desktop"]
    MODE -->|KIOSK| K["KIOSK= .XAPP / Full"]
    MODE -->|IOT| I["Background / Screen=None"]
    MODE -->|AUTO| A{"KIOSK set?"}
    A -->|Yes| K
    A -->|No| D
    K --> EXIT{"KIOSK_EXIT"}
    EXIT -->|RESTART| K
    EXIT -->|DESKTOP| D
    EXIT -->|REBOOT| CFG
~~~

### Arduino XAPP architecture

~~~mermaid
flowchart LR
    IDE["Arduino IDE + RPiDuino"] --> SK["Sketch: setup / loop"]
    SK --> BUILD["Pi 3/3B+ · XDOS XAPP"]
    BUILD --> FILE["V2 .XAPP + optional .APP"]
    FILE --> LOAD["XDOS loader / ARM32 imports"]
    LOAD --> SCREEN["Screen=Full / Window / None"]
    LOAD --> SERVICE["XDOS API / CPU0 services"]
~~~

### microSD installation and updates

~~~mermaid
flowchart TD
    PKG["Release files + SHA-256"] --> PLAN{"Install or update?"}
    PLAN -->|First install| SD["FAT boot partition + Pi firmware"]
    SD --> COPY["Copy IMG / XAPP; set kernel= in config.txt"]
    PLAN -->|Kernel update| NET["UART NET → LAN transfer or UART PUT"]
    NET --> CHECK["CRC32 → BOOT → RESET"]
    COPY --> RUN["Pi 3B+: boot + PING READY"]
    CHECK --> RUN
    PLAN -->|Arduino XAPP only| APP["Copy or upload .XAPP; install .APP if needed"]
    APP --> TEST["Launch from XDOS"]
    RUN -->|Failure| RB["Restore previous kernel and config"]
~~~


### Documentation languages and runtime fonts

~~~mermaid
flowchart LR
    DOC["Public documentation"] --> EN["English"]
    DOC --> TC["繁體中文"]
    DOC --> JP["日本語"]
    SD["microSD / FONTS"] -. "experimental" .-> FONT["LVGL Traditional Chinese fallback"]
    FONT -.-> UI["LVGL XAPP text and FVLANG1 labels"]
~~~

![Traditional Chinese desktop on Pi 3B+ (FVLANG1 test build; LAN address masked)](fvlang1/desktop-zh-tw.png)

Traditional Chinese desktop on Pi 3B+ (FVLANG1 test build; LAN address masked)

### Development and release flow

~~~mermaid
flowchart LR
    SRC["Git source + dependency revisions"] --> BUILD["WSL cross-build"]
    BUILD --> ART["IMG / XAPP / SHA-256"]
    ART --> HW["Pi 3B+ / UART / HDMI"]
    HW -->|Pass| PUB["GitHub main + Release"]
    HW -->|Fail| RB["Rollback and investigate"]
~~~
