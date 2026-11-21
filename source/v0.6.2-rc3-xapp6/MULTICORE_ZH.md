# XDOS V0.6.2-RC3 XAPP6 MultiCore（Raspberry Pi 3B+）

發行日期：2026-11-21
核心版本字串：`V0.6.2-RC3 XAPP6-PERF7-VC4-AnalyticsRegionGuard-MultiCore / Pi 3 ARM32`
核心檔名：`XDOSMC.IMG`

此為獨立的 Pi 3B+ ARM32 有線效能預發行版。PXLAYOUT 為另一條桌面與媒體版本線，兩版的核心綁定 XAPP 各自配對。以 PERF7 VC4 AnalyticsRegionGuard 為基礎，重整 LVGL 的刷新路徑與計時，並改由 4 核心分工繪圖。

## 效能

在 Pi 3B+ 上以 1920×1080 60 Hz（`HDMI-MODE 1080P`）執行官方 LVGL Benchmark，原版與本版使用相同的顯示設定。

| 場景（FPS） | 原版 HDBENCH | MultiCore |
|---|---|---|
| Single rectangle | 49 | **60** |
| Multiple rectangles | 27 | **60** |
| Empty screen | 22 | **51** |
| Multiple RGB images | 15 | **52** |
| Multiple ARGB images | 14 | **49** |
| Rotated ARGB images | 12 | **47** |
| Multiple labels | 22 | **50** |
| Screen sized text | 10 | **19** |
| Multiple arcs | 11 | **35** |
| Containers | 10 | **60** |
| Containers with overlay | 8 | **19** |
| Containers with opa | 9 | **59** |
| Containers with opa_layer | 7 | **41** |
| Containers with scrolling | 10 | **44** |
| Widgets demo | 35 | **45** |
| **全部場景平均** | **17** | **46** |
| 平均 Render / Flush | 23 / 44 ms | 11 / 8 ms |

- 輕量場景已達 60 Hz VSync 上限。Flush 的 8–14 ms 是在等 VSync，屬正常。
- 桌面閒置 CPU 約 11%。XAPP 開著但畫面靜止時約 3%（以同一程式碼的 TuneD 測試版量測）。
- 原始數據：`benchmark-1080p60-multicore.csv`、`benchmark-1080p60-original.csv`。

## 變更內容

1. **VC4 刷新路徑**
   - 每幀只做一次 `glGetError`，原本每個區塊做一次。
   - 移除換頁前多餘的 `glFinish`。
   - EGL 畫面緩衝改為 RGB565，dispmanx 圖層設為不透明。
   - VC4 呼叫期間，VCHIQ 等待改為讓出 CPU，不必等下一個 100 Hz 計時中斷。

   這幾項讓 Flush 從 44 ms 降到個位數。
2. **LVGL 時鐘**：改由 64 位元微秒計時器提供 1 ms 精度，原本是 100 Hz 中斷每次加 10 ms。這解除了 50 FPS 的上限，刷新與動畫週期設為 16 ms。
3. **XAPP 執行時精準喚醒**：主迴圈先讓 USB、網路等工作執行，再以 `WFE` 睡到 LVGL 下一次到期。喚醒來源是 ARM 通用計時器事件串流，約 0.43 ms 一次，所以既準時又省電，睡眠時間也計入 CPU% 的閒置。回到桌面後沿用原本的省電路徑。
4. **多核繪圖**：開啟 Circle `ARM_ALLOW_MULTI_CORE`，以自訂的 `LV_OS_CUSTOM` 層把 3 個 LVGL 軟體繪圖單元固定在 core 1–3。Core 0 保留 Circle 排程器、USB、網路、UART、VC4 與 LVGL 主流程。不切 tile（`tile_cnt=1`），以不同物件為平行單位，避免文字被重複排版。
5. **1080p 捲動快取**：官方 Benchmark 捲動場景的 GPU 紋理重用上限，從 1024×768 放寬到 1920×1080。GPU 搬移後先等待完成，再上傳新露出的區帶，修正了 1080p 畫面下方的殘影。
6. **LVGL 設定**：透明圖層緩衝 24→512 KiB，陰影快取 128。
7. **UART `HDMI-MODE 720P|1080P`**：會寫入 `config.txt` 的 HDMI 模式與 framebuffer 尺寸，重開後生效。

## 檔案

| 檔案 | 說明 |
|---|---|
| `XDOSMC.IMG` | 核心 |
| `LVGLDEMO.XAPP`、`LVGLBENCH.XAPP`、`BROWSER.XAPP`、`TERMINAL.XAPP` | 與本核心綁定的 XAPP。放到 `SD:/XDOS/INBOX/` 時依序命名為 `MCWIDGET.XAPP`、`MCBENCH.XAPP`、`MCBROWSE.XAPP`、`MCTERM.XAPP` |
| `CALC`、`NOTE`、`PAINT`、`FILES` 的 `.APP`／`.XAPP` | 桌面應用，放在 `SD:/XDOS/APPS/` |
| `XDOS-MultiCore-Pi3B-Plus-SD.zip` | SD 卡檔案：`XDOSMC.IMG`、`XDOS/APPS/`、`XDOS/INBOX/`（已依上表命名） |
| `XDOS-MultiCore-source.zip` | 建置腳本、原始碼及公開清理後的 RC3 基線 |
| `SHA256SUMS.txt` | 以上檔案的 SHA-256 |

## 安裝

### 首次安裝（SD 卡）

1. 準備已有 Raspberry Pi 開機韌體的 FAT 開機分割區，並備份 `config.txt`。
2. 把 `XDOS-MultiCore-Pi3B-Plus-SD.zip` 解壓到分割區根目錄。
3. 在 `config.txt` 的通用區段設定 `kernel=XDOSMC.IMG`。要固定 1080p，可另外加入：

   ```
   hdmi_force_hotplug=1
   hdmi_group=1
   hdmi_mode=16
   disable_overscan=1
   framebuffer_width=1920
   framebuffer_height=1080
   ```

### 從已安裝的有線版升級（網路 + UART）

需要支援 `FETCH`、`BOOT`、`RESET` 的 XDOS 有線版。從完整原始碼包取得 tools 目錄，以下命令假設 tools 和發行附件位於同一工作目錄。請保留目前的核心與配對 XAPP，以便回退。

```powershell
$uart = 'python'
$common = @('tools/xdos-uart.py','--port','SERIAL_PORT','--baud','921600')
$lan  = '<電腦的區網 IPv4>'
& $uart @common net-scp LVGLDEMO.XAPP  --name MCWIDGET.XAPP --bind $lan
& $uart @common net-scp LVGLBENCH.XAPP --name MCBENCH.XAPP  --bind $lan
& $uart @common net-scp BROWSER.XAPP   --name MCBROWSE.XAPP --bind $lan
& $uart @common net-scp TERMINAL.XAPP  --name MCTERM.XAPP   --bind $lan
& $uart @common net-scp XDOSMC.IMG     --name XDOSMC.IMG    --bind $lan
& $uart @common boot    XDOSMC.IMG     --name XDOSMC.IMG
& $uart @common reset
```

在 PowerShell 安裝 pyserial，把 SERIAL_PORT 和區網位址替換成自己的值。每個傳檔指令都要先看到「CRC32 verified」再執行下一個。網路傳送被拒時，可把 `net-scp ... --bind $lan` 換成 UART 的 `scp`。

開機後若要固定 1080p，執行 `@XDOS HDMI-MODE 1080P` 再 `reset`。`tools/uart_cmd.py` 可送出這個指令。

### 回退

在 UART 可回應時執行 `boot-clear` 再 `reset`。若無法開機，取出 microSD，把 `config.txt` 的 `kernel=` 改回原核心。

## 驗證紀錄（Pi 3B+）

- 正式建置（`XDOS_TEST_DEBUG=0`）以網路部署，5 個檔案 CRC32 全部通過，1080p 正常開機並跑完官方 Benchmark。
- 同一套程式碼的測試版（TuneC／TuneD）已實機確認：滑鼠鍵盤、各 XAPP（Browser、Terminal、Widgets、Benchmark）、網路正常，桌面操作更順，1080p 捲動無殘影。

## 已知限制

- 只有有線網路，不含 Wi-Fi，與 PERF7 Wired 相同。
- 1080p 的 GPU 捲動快取只用於官方 Benchmark 的卡片捲動場景。Widgets Analytics 沿用 RegionGuard。
- 文字最重的場景（Screen sized text 19 FPS）和多層透明疊加（Containers with overlay 19 FPS）仍以 CPU 繪圖為瓶頸。
- 右上角 CPU% 只反映 core 0，繪圖核心 1–3 的負載不計入。
- 欠壓會讓 Pi 降頻，Pi 3B+ 經 USB 的乙太網路也可能中斷。若 `power` 指令顯示曾欠壓，請檢查電源。

## 從原始碼建置

解開完整的 XDOS-MultiCore-source.zip，進入含 tools、arduino-demos 和 RC3 基線 ZIP 的目錄，在 WSL 安裝 ARM32 交叉編譯工具後執行：

```bash
XDOS_VC4_TUNE=1 XDOS_WIRED_ONLY=1 XDOS_ANALYTICS_MODE=analyticsregion XDOS_BROWSER_XAPP=1 \
XDOS_TEST_DEBUG=0 XDOS_HDMI_MODES_TEST=1 \
XDOS_TUNE_AWAKE=1 XDOS_TUNE_TICK=1 XDOS_TUNE_SCROLL_HD=1 \
XDOS_TUNE_MULTICORE=3 XDOS_TUNE_TILES=1 \
XDOS_TUNE_TAG=M XDOS_TUNE_LABEL=MultiCore XDOS_TUNE_PREFIX=MC XDOS_TUNE_IMG=XDOSMC.IMG \
bash tools/build-wsl.sh "$PWD"
```

公開原始碼包使用已清理本機資訊的 RC3 基線封存檔，對應的共通來源檔與原建置輸入逐檔一致。重新打包會改變基線 ZIP 雜湊。附帶的預編譯 IMG 和 XAPP 保持原驗收位元組。

建置環境：ARM GCC 13.2.1、Circle `7a81e1b480fe7d19fc1fc695b89bff0a0848f44b`、LVGL 9.4。
