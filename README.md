# Raspberry Pi XDOS

XDOS 是將 XD OS 移植到 Raspberry Pi 的 ARM32 裸機桌面系統。以 Circle 提供硬體支援，使用 LVGL 繪製桌面，並從 microSD 載入外部 XAPP。近期版本以 Raspberry Pi 3B+ 為實機驗證平台，搭配 Windows WSL2 交叉編譯。

**目前的預發行版是 Pixel BBS Layout。** Pi 3B+ 已在 720p 和 1080p 驗證自適應圖示、文字、間距及九個捷徑。Release 提供核心 IMG、配對 XAPP、原始碼快照與 SHA-256。 [下載 PXLAYOUT](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-pxlayout)

## 開始使用

| 需求 | 文件 |
| --- | --- |
| PXLAYOUT 實測、microSD 安裝、部署、播放器及回退 | [Pixel BBS Layout 指南](source/v0.6.2-rc3-xapp6/PXLAYOUT_ZH.md) |
| WSL2 建置、首次安裝到 microSD、桌面操作 | [XDOS 安裝與操作指南](source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) |
| 使用 Terminal、查詢效能、從 UART 啟動 XAPP | [Terminal Window 操作說明](source/v0.6.2-rc3-xapp6/TERMINAL_ZH.md) |
| UART 指令、協定與新增指令 | [UART 指令參考](source/v0.6.2-rc3-xapp6/UART_ZH.md) |
| 區網傳檔、選擇開機映像、RESET 與回退 | [區網部署指南](source/v0.6.2-rc3-xapp6/NETWORK_UPDATE_ZH.md) |

首次安裝時，將 IMG 複製到已有 Raspberry Pi 開機韌體的 FAT 開機分割區，設定 `config.txt` 的 `kernel=`，並安裝同一 Release 的 XAPP。這裡的 IMG 是裸機核心檔。完整步驟與回退方式見上表。

## 研發紀錄

| 階段 | 里程碑 |
| --- | --- |
| V0.1–V0.3 · ARM32 移植 | V0.1 顯示靜態 HDMI framebuffer 桌面。V0.2 驗證 USB 鍵鼠與視窗操作。V0.3 加入 RAM back buffer、局部重畫、有線網路與 HTTP Browser。當時使用者回報從開機畫面到桌面約 3 秒。 |
| V0.4–V0.5.4 · 網路與儲存 | 開發 Wi-Fi 和 HTTP 逾時，加入開機 Splash、Ethernet／Wi-Fi 選擇、優先顯示桌面與 FAT32。V0.5、V0.5.2 已從舊 Pi 4 找回原始碼與核心。V0.4 有明確標示的重建版，尚未實機開機。V0.5.1 尚無可驗證原檔。 |
| V0.6.1–RC3 XAPP6 · LVGL 與外部程式 | 整合 LVGL 局部重畫。官方 Widgets 與 Benchmark 改由外部 XAPP 載入，執行區在開機時保留於 RAM。 |
| RegionGuard · VideoCore IV | VC4 GLES2 呈現搭配 EGL VSync，保留 VSyncOpt 回退。捲動快取遇到重繪區和快取重疊時採用保守重畫，避免漏圖和殘影。 |
| AppWindows、BrowserText、Wired · 核心精簡 | 桌面工具改成視窗版 XAPP，補齊文字鍵，將 Browser HTTP 和 HTML 轉文字移到 XAPP。核心保留區網更新，有線版移除內建 Wi-Fi。 |
| HDMI Auto · 解析度與顯示 | 讓 framebuffer、XDOS、LVGL 和 VC4 使用同一尺寸。720p 桌面已驗證。1080p 的官方 Benchmark 圖形上捲修正版已確認無殘影，EGL VSync 保持啟用。 |
| Terminal Window · 共用指令 | Terminal 可用桌面捷徑或 UART 啟動，沿用視窗縮放動畫和 CPU 顯示。Terminal 與 UART 共用核心指令表。 |
| Pixel BBS Layout · 自適應桌面 | 開機進度條和桌面共用 8×8 顆粒主題。Pi 3B+ 的 720p 與 1080p 各完成九個圖示及九個名稱的滑鼠啟動測試。 |

版本細節、歷史檔案與驗證狀態見 [版本總覽](VERSIONS.md)、[早期版本復原紀錄](archive/PI4_RECOVERY_ZH.md) 和 [HDMI Auto 實測說明](source/v0.6.2-rc3-xapp6/HDMI_AUTO_ZH.md)。早期 V0.1–V0.3 建置與開機方式見 [歷史操作手冊](OPERATIONS.md)。

原始碼與文件維護於 `main`，各版以 Git 標籤和 Releases 查找。[發布流程](RELEASING.md) · [授權及第三方元件](LICENSE.md)
