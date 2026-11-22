# XDOS 版本總覽

程式、操作說明和版本紀錄統一維護於 `main`。以 Git 標籤定位版本，以 GitHub Releases 下載配對映像及 XAPP。

| 版本 | 公開資料與狀態 |
| --- | --- |
| V0.4 | [V0.4 重建版](archive/reconstructed/v0.4/) 有原始碼、Ethernet／Wi-Fi IMG、SHA-256 及差異說明。原發行檔尚未找回，重建映像尚未實機開機。 |
| V0.5 | [從 Pi 4 找回的原始碼與核心映像](archive/XDOS-V0.5-Pi4-recovered.zip)。原發行 ZIP 尚未找回。 |
| V0.5.1 | 有修正版檔名紀錄。尚未找到可驗證的原檔，也缺少足以辨識此版改動的證據。 |
| V0.5.2 | [從 Pi 4 找回的原始碼與核心映像](archive/XDOS-V0.5.2-Pi4-recovered.zip)。原發行 ZIP 尚未找回。 |
| V0.5.3 | [Desktop-first 原始碼](source/v0.5.3/) |
| V0.5.4 | [FAT32 歷史套件](archive/XDOS_ARM32_ASM_V0.5_4_FAT32_RPi3B-sanitized.zip) |
| V0.6.1 | [LVGL 效能歷史套件](archive/XDOS_ARM32_V0.6.1_LVGL_PERF_MULTI_RPI-sanitized.zip) |
| V0.6.2-RC2 | [Pi 3 測試套件](archive/XDOS_ARM32_V0.6.2_RC2_PI3_TESTKIT-sanitized.zip) |
| V0.6.2-RC3 HF7 | [外部 LVGL Demo 開發紀錄](docs/XDOS_V0.6.2_HF7_PUBLIC_STATUS.md)，保留當時驗證狀態 |
| XAPP6 | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6)。官方 Widgets 與 Benchmark 外部化，執行區在 RAM。 |
| RegionGuard | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-regionguard)。VC4 捲動與 EGL VSync，保留 VSyncOpt 回退。 |
| AppWindows-CoreUpdate | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-appwindows-coreupdate)。視窗版 XAPP 與核心更新。 |
| AppWindows-BrowserKeys | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-appwindows-browserkeys)。Browser HTTP 外移、鍵盤補齊。 |
| BrowserText | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-browsertext)。Browser HTML 轉文字外移。 |
| Wired | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-wired)。有線精簡核心。 |
| HDMI Auto | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-hdmi-auto)。解析度同步、視窗動畫和 1080p 捲動保護。 |
| Terminal Window | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-terminal-window)。外部 Terminal XAPP、共用 UART 指令表。 |
| PXLAYOUT | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-pxlayout) · [操作與實測](source/v0.6.2-rc3-xapp6/PXLAYOUT_ZH.md)。Pi 3B+ 的 720p、1080p 自適應版面及滑鼠捷徑均通過。 |
| MultiCore Pi 3B+ | [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-multicore) · [繁體中文](source/v0.6.2-rc3-xapp6/MULTICORE_ZH.md) · [English](source/v0.6.2-rc3-xapp6/MULTICORE_EN.md)。獨立有線效能預發行版，提供四核心分工、VC4 刷新優化及 1080p 原始 Benchmark CSV。 |

歷史 ZIP 已移除或改用範例值處理本機路徑與 Wi-Fi 設定。[歷史 ZIP 校驗表](archive/SHA256SUMS.txt)

PXLAYOUT 之後的媒體測試、限制及回退狀態見[研發與驗證紀錄](source/v0.6.2-rc3-xapp6/DEVELOPMENT_ZH.md)。

V0.5 和 V0.5.2 的來源辨識、核心雜湊與使用方式見[早期版本復原紀錄](archive/PI4_RECOVERY_ZH.md)。

**FILEQ128 預覽版與 RPiDuino 0.1.4** · [安裝、更新與研發紀錄](docs/fileq128.zh-TW.md) · [Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-fileq128)

## FILECLR

[Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-fileclr) · [繁體中文](source/v0.6.2-rc3-xapp6/FILECLR_ZH.md) · [English](source/v0.6.2-rc3-xapp6/FILECLR_EN.md)
