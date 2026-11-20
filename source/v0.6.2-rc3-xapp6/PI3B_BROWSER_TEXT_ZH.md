# XDOS BrowserText · Pi 3B+ 實測

Raspberry Pi 3B+ ARM32 預發行版。核心與 Browser、Widgets、Benchmark 三個配對 XAPP 經區網部署，四個檔案均由 Pi 校驗 CRC32。HTML 轉文字在 Browser XAPP 內執行，核心接收轉換後的文字和狀態。

| 項目 | 結果 |
| --- | --- |
| Browser | 開啟視窗時無 GET。提交測試網址後收到 HTTP 200。HDMI 正常顯示標題和 Alpha & Beta，script／style 內容未顯示。 |
| HTML 轉換 | WSL 以 3,000 組測試輸入比對，外移前後的輸出一致。 |
| 測試 DEBUG | XDOS_TEST_DEBUG=0。IMG 不含被關閉的測試追蹤字串，UART 開機與 Browser 請求期間也未輸出這些訊息。 |
| 核心 FETCH | 區網端點收到 GET，Pi 回報 FETCH DONE。第一次重開後探測遭拒，沿用已校驗的映像重試後通過。 |
| 核心 BOOT／RESET | BOOT 驗證新映像 CRC32。RESET 後回到 BrowserText，UART PING 和 POWER 回應正常。 |

## 安裝與回退

下載 IMG、BROWSER.XAPP、LVGLDEMO.XAPP、LVGLBENCH.XAPP 與 SHA256SUMS.txt，核對後備份舊映像、config.txt 和舊版配對 XAPP。將 IMG 複製到 microSD 開機分割區根目錄，讓 config.txt 的 kernel 指向它，並將三個配對 XAPP 放入 XDOS/APPS/，沿用既有的同名 APP 捷徑。

使用 UART 指揮區網更新時，將 IMG 上傳為 BRTEXT.IMG。Browser、Widgets、Benchmark 的別名依序為 T3WEB.XAPP、T3WIDGET.XAPP、T3BENCH.XAPP。UART 使用 921600 baud、8N1。

[安裝與 UART 操作指南](https://github.com/hypersolver/raspberry-pi-xdos/blob/main/source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) · [BrowserKeys R2 回退版](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-appwindows-browserkeys)
