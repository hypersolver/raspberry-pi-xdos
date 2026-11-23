# XDOS 架構與研發紀錄

[English](architecture.md) · [日本語](architecture.ja.md) · [版本總覽](../VERSIONS.md)

## 已發布版本與實驗進度

| 版本 | 實測紀錄 |
| --- | --- |
| FVR17 | V2 模組可跨相容核心綁定匯入符號，同名同內容可重複上傳，同名異檔會被拒絕。 |
| FVR18 | Pi 3B+ 驗證四核心使用率與執行中的 XAPP 名稱。 |
| FVR19 | 開機模式、AUTORUN、KIOSK 退出及回退共 44 項 UART 檢查通過。 |
| FVR20 | 921600 baud、400 MHz 固定時脈、UART／有線網路 CRC32 與長目錄清單通過。 |
| FVR21 | 唯讀 UART NET 可查即時 IPv4、遮罩、閘道與取址模式。DHCP 與主機網卡選擇已驗證，靜態 IP 和拔線未上機驗證。 |

FVR19 的 1080p LVGL 基線為整體 41 FPS、捲動容器 43 FPS。滿版文字和容器疊層仍待優化。局部重繪、字型、圖片解碼及其他 V2 移植已有實驗建置，依各項驗證結果個別整合與發行。

## 架構與燒錄流程圖

CPU0 負責硬體 I/O 和 UI 事件迴圈。CPU1–CPU3 可繪製 LVGL，或借給符合條件的 XAPP 運算。V2 最多可同時啟用四槽，其中可用 CPU0 的共享模式。UI 與裝置服務依核心的 API 與執行核心限制使用。

公開文件提供英語、繁體中文和日語。測試版的 LVGL 繁體中文字型從 microSD 載入並附授權；FVLANG1 另有實驗桌面語系功能，可從 microSD 載入字典並翻譯圖示名稱。日文文件已提供，日文執行時語系包尚未實機驗證。

FVR21 的 AUTO 依 KIOSK 欄位選擇模式，不讀取 HDMI 狀態。各模式皆可用 AUTORUN 啟動背景 XAPP。IOT 暫停 LVGL 更新並限制 Screen=None。KIOSK 啟動一個全螢幕 XAPP，結束時依設定處理。

Arduino 請使用 [RPiDuino SDK](https://github.com/hypersolver/RPiDuino)，選 Pi 3/3B+ 的 XDOS XAPP 模式。首次安裝將 IMG 複製到已有 Pi 韌體的 FAT 開機分割區；後續 Arduino 草稿編成 XAPP，部署到既有 XDOS。完整步驟見[操作指南](../source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md)。

### 系統總覽

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

### 多 CPU 排程

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

### 桌面、KIOSK 與 IOT 模式

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

### Arduino XAPP 架構

~~~mermaid
flowchart LR
    IDE["Arduino IDE + RPiDuino"] --> SK["Sketch: setup / loop"]
    SK --> BUILD["Pi 3/3B+ · XDOS XAPP"]
    BUILD --> FILE["V2 .XAPP + optional .APP"]
    FILE --> LOAD["XDOS loader / ARM32 imports"]
    LOAD --> SCREEN["Screen=Full / Window / None"]
    LOAD --> SERVICE["XDOS API / CPU0 services"]
~~~

### microSD 燒錄與更新

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


### 文件語系與執行時字型

~~~mermaid
flowchart LR
    DOC["Public documentation"] --> EN["English"]
    DOC --> TC["繁體中文"]
    DOC --> JP["日本語"]
    SD["microSD / FONTS"] -. "experimental" .-> FONT["LVGL Traditional Chinese fallback"]
    FONT -.-> UI["LVGL XAPP text and FVLANG1 labels"]
~~~

![Pi 3B+ 繁體中文桌面（FVLANG1 測試版，區網位址已遮蔽）](fvlang1/desktop-zh-tw.png)

Pi 3B+ 繁體中文桌面（FVLANG1 測試版，區網位址已遮蔽）

### 開發與發布流程

~~~mermaid
flowchart LR
    SRC["Git source + dependency revisions"] --> BUILD["WSL cross-build"]
    BUILD --> ART["IMG / XAPP / SHA-256"]
    ART --> HW["Pi 3B+ / UART / HDMI"]
    HW -->|Pass| PUB["GitHub main + Release"]
    HW -->|Fail| RB["Rollback and investigate"]
~~~
