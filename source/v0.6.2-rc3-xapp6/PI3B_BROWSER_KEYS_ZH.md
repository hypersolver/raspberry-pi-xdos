# XDOS AppWindows-BrowserKeys R2 · Pi 3B+ 實測

Raspberry Pi 3B+ ARM32 預發行版。核心 IMG 與 Browser、Widgets、Benchmark 三個配對 XAPP 經區網更新，Pi 完成 CRC32 校驗並重新開機。UART 確認 R2 版本、GLES2 EGL 同步、電源狀態正常以及 Notepad 已啟動。

## 實體鍵盤逐鍵驗收

在 R2 的 Notepad 以兩行測試文字逐鍵操作。八個按鍵均通過。

| 按鍵 | 驗收結果 |
| --- | --- |
| ↑、↓ | 游標在上下兩行間移動 |
| ←、→ | 游標在目前行內向左與向右移動 |
| Home、End | 到目前行的行首與行尾 |
| Delete | 刪除游標後的字元 |
| Insert | 切換覆寫與插入，兩種模式均通過 |

R2 修正 Home／End 的行內行為，並避免覆寫模式跨過換行。預設鍵盤配置為 US，保留開機選項覆寫。前一版實機已確認實體鍵盤可輸入斜線。

## Browser HTTP 與更新

Browser HTTP 實作由配對的 Browser XAPP 提供，核心啟動時不建立常駐 HTTP worker。前一版 BrowserKeys 在 Pi 3B+ 已驗證送出網址後才發出 GET 並取得 HTTP 200。前一版也已實測核心 FETCH、BOOT、RESET。R2 本輪的實機驗收以區網部署、開機與鍵盤為主。

## 安裝與回退

先備份現有開機映像、config.txt 與配對 XAPP。校驗 SHA-256 後將本版 IMG 放到 microSD FAT 開機分割區根目錄，讓 config.txt 的 kernel 指向它。三個 XAPP 放入 XDOS/APPS/，沿用既有同名的 APP 捷徑。使用 UART 區網更新時，三個上傳別名依序為 B2WEB.XAPP、B2WIDGET.XAPP、B2BENCH.XAPP。

[安裝與 UART 操作指南](https://github.com/hypersolver/raspberry-pi-xdos/blob/main/source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) · [AppWindows-CoreUpdate 回退版](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-appwindows-coreupdate)
