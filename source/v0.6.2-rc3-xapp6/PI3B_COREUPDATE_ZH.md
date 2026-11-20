# XDOS V0.6.2-RC3 XAPP6 · AppWindows-CoreUpdate

Raspberry Pi 3B+ ARM32 預發行測試版。桌面 CPU 讀值已在 Pi 3B+ 驗證，核心、Widgets 與官方 LVGL Benchmark XAPP 經區網傳送並由 Pi 核對 CRC32 後重新開機。

## Pi 3B+ CPU 實測

| 約 5 秒的 UART 取樣 | CPU 使用率 |
| --- | --- |
| 原 AppWindows 桌面閒置 | 100%、100%、100% |
| CoreUpdate 開機初期 | 37% |
| CoreUpdate 桌面穩定閒置 | 12%、12% |
| CoreUpdate Calculator 視窗靜置 | 11%、11%、11% |
| CoreUpdate Calculator 拖曳與按鍵 | 13%、13%、13%、18% |

VCHIQ keep-alive 在等待 completion 事件時反覆執行 `Yield()`，使任務保持可執行，排程器無法進入 WFI。修正後，無逾時的 completion 等待可短暫睡眠，CPU 計算公式維持原樣。新版桌面穩定閒置時，每約 5 秒有 4.44 秒處於 WFI。開機、閒置與視窗操作時的供電狀態均為 `0x00000000`。

本版移除 XDOS UPDATE 桌面捷徑、`OPEN UPDATE` 與 UPDATE XAPP 建置產物。核心保留 UART `FETCH`、`BOOT` 和 `RESET`，新版上板已確認 `FETCH-STATUS` 正常回應。本輪 CPU 驗收的視窗為 Calculator。

## 安裝與回退

下載核心 IMG、`LVGLDEMO.XAPP`、`LVGLBENCH.XAPP`，核對 `SHA256SUMS.txt`。沿用既有 XDOS microSD 上的其他 XAPP 與捷徑。先備份原核心、`config.txt` 和配對 Demo，將 IMG 複製到 FAT 開機分割區根目錄，並將 `config.txt` 的 `kernel=` 指向新 IMG。兩個配對的 XAPP 放入 `XDOS/APPS/`，沿用原本同名的 `.APP` 捷徑。

透過區網部署時，Pi 的兩個上傳檔名使用 `CUWIDGET.XAPP` 與 `CUBENCH.XAPP`。UART 設定為 921600 baud、8N1。回退時請一併恢復舊版核心和它配對的 XAPP。

[安裝與 UART 操作指南](https://github.com/hypersolver/raspberry-pi-xdos/blob/main/source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) · [原 AppWindows 測試包](https://github.com/hypersolver/raspberry-pi-xdos/tree/main/source/v0.6.2-rc3-xapp6/packages) · [RegionGuard 回退版](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-regionguard)
