# XDOS V0.6.2-RC3 XAPP6 · VC4 AnalyticsRegionGuard

Raspberry Pi 3B+、ARM32 裸機測試版。已實機完成 Widgets Analytics 連續捲動，HDMI 確認圖形完整，無殘影或水平撕裂。

## Pi 3B+ 實測比較

兩版使用相同捲動場景。量測前後電源狀態皆為 `0x00000000`。

| 指標 | AnalyticsSafe | RegionGuard（HDMI 重播） |
| --- | ---: | ---: |
| 每幀平均紋理上傳時間 | 1.501 ms | 1.487 ms |
| 平均幀間隔 | 25.125 ms | 25.129 ms |
| 幀間隔 P95 | 40.749 ms | 40.749 ms |
| GLES2 呈現幀數 | 207 | 207 |
| 手動 VSync | 0 | 0 |
| 手動 framebuffer 換頁 | 0 | 0 |
| 呈現錯誤 | 0 | 0 |

當待重畫區與可重用區相交時，RegionGuard 完整重畫捲動容器。這個做法修復測試場景中先前觀察到的殘影與圖形缺失，效能與 AnalyticsSafe 接近。GPU 正常時使用 EGL 同步，`eglSwapInterval(1)`。VSyncOpt 回退仍保留，本輪未觸發。

## 安裝與回退

下載核心 IMG、`LVGLDEMO.XAPP`、`LVGLBENCH.XAPP`，核對 `SHA256SUMS.txt`。先備份 microSD 的 `config.txt`、原核心與配對 XAPP。將 IMG 放到已有 Raspberry Pi 開機韌體的 FAT 開機分割區根目錄，將兩個 XAPP 放在 `XDOS/APPS/` 並沿用原本的對應 `.APP` 捷徑。將 `config.txt` 的 `kernel=` 指向新 IMG。IMG 是裸機核心檔，可直接複製到開機分割區。

若用支援區網更新的 UART 韌體部署，交由 Pi 校驗 CRC32。兩個 XAPP 的上傳檔名分別用 `R4WIDGET.XAPP`、`R4BENCH.XAPP`，再選擇核心與 RESET。UART 使用 921600 baud、8N1。

要回退至原卡上的 AnalyticsSafe，將 `kernel=` 指回保留的舊 IMG，並使用該版配對的兩個 XAPP。首次安裝與 UART 操作見 [繁體中文操作指南](https://github.com/hypersolver/raspberry-pi-xdos/blob/main/source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md)。
