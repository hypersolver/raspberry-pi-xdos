# XDOS 有線精簡版 · Pi 3B+ 實測

核心移除內建 Wi-Fi，保留有線 Ethernet、SD／FAT32、USB 鍵鼠與隨身碟、VC4 EGL VSync 和 VSyncOpt 回退。測試訊息以 XDOS_TEST_DEBUG=0 關閉。

| 核心 | IMG |
| --- | ---: |
| BrowserText | 1,852,228 bytes |
| Wired | 1,434,092 bytes |
| 減少 | 418,136 bytes（408.3 KiB，22.6%） |

Pi 3B+ 透過區網取得核心及配對的 Browser、Widgets、Benchmark XAPP，檔案通過 CRC32。核心 FETCH、BOOT、RESET 已驗證。Widgets 和官方 Benchmark 已確認可啟動。Browser 以有線網路取得 HTTP 200，實機確認文字及 USB 鍵鼠操作正常。Files 可列出 FAT32 隨身碟。VSyncOpt 回退保留，本輪未強制觸發。

## 安裝、操作與回退

1. 從同一 Release 下載 IMG、三個 XAPP 和 SHA256SUMS.txt，核對後備份原核心、config.txt 與配對 XAPP。
2. 首次以 microSD 安裝時，將 IMG 複製到含 Raspberry Pi 開機韌體的 FAT 分割區根目錄，讓 config.txt 的 kernel 指向新檔。三個 XAPP 放入 XDOS/APPS/，沿用原同名 APP 捷徑。其餘應用程式可從基準完整測試包取得。
3. 使用 UART 指揮區網更新時，核心檔名用 WIRED.IMG，Browser、Widgets、Benchmark 分別用 W3WEB.XAPP、W3WIDGET.XAPP、W3BENCH.XAPP。UART 使用 921600 baud、8N1。逐個傳檔並確認 CRC32，再執行 boot 與 reset。
4. 開機後用有線 Ethernet 取得網路。Browser 開啟後輸入 HTTP 網址並按 OPEN，Files 可用 LIST SD 或 LIST USB。Widgets 和官方 Benchmark 可從桌面捷徑開啟。
5. 需要回退時，選回 BrowserText 核心並配對該版 XAPP。若 UART 無法使用，關機後手動還原 microSD 的 config.txt。

[完整操作指南](https://github.com/hypersolver/raspberry-pi-xdos/blob/main/source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) · [BrowserText 回退版](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-browsertext)
