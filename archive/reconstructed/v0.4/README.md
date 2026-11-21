# XDOS V0.4 重建版

從已找回的 V0.5 原始碼重建。舊對話將 Wi-Fi 和 Browser Timeout 歸於 V0.4，將開機 Splash 和分階段診斷歸於 V0.5。兩份 IMG 都以 `V0.4 RECONSTRUCTED` 標示。原始 V0.4 IMG 尚未找到，這兩份重建映像未在 Pi 3B+ 開機驗證。

| 項目 | 重建方式 |
| --- | --- |
| 桌面和 HTTP Browser | 沿用 V0.5 找回的程式。保留其 HTTP 總逾時和讀取閒置逾時。 |
| 網路 | 沿用 V0.5 的編譯期選擇，提供 Ethernet 與 SDIO Wi-Fi 各一份 IMG。兩種模式是否與原 V0.4 發行規格完全相同，沒有原檔可核對。 |
| 開機 | 移除 V0.5 的 Splash、分階段診斷和 Splash 停留。保留基本 UART 初始化錯誤報告與 V0.5 的 1.2 秒網路等待。 |
| TCP 補丁 | 使用 Pi 4 上現存的 [Circle 差異](circle-tcp-timeout.patch)，握手等待約 5 秒，失敗後清理連線。兩份 V0.4 備份與上游原檔的 SHA-256 相符。已檢查編譯用的物件含有逾時呼叫。 |

## 建置與安裝

在 Pi 4 以 GNU Arm 工具鏈 13.2.1 建置，Circle 基線為 `7a81e1b480fe7d19fc1fc695b89bff0a0848f44b`，使用 ARM32、`RASPPI=3`。將 `source/` 複製到 Circle 的 `sample/` 下獨立目錄，在 Circle 使用上述補丁並建好 Makefile 所需靜態庫，再分別以 `make WIFI=0` 和 `make WIFI=1` 編譯。兩個模式請使用各自的建置目錄。建置產生 `kernel8-32.img`。

測試前先用 [SHA-256](SHA256SUMS.txt) 核對 IMG。備份 Pi 3 的 microSD，將所選 IMG 複製到已有相容開機韌體的 FAT 分割區，在 `config.txt` 設定 `arm_64bit=0` 並以 `kernel=` 指向該映像。Wi-Fi 版另需相容的 WLAN 韌體與自己的 `wpa_supplicant.conf`。UART 使用 115200 baud。

## V0.5.1

目前有舊套件名稱及 V0.5 和 V0.5.2 的可驗證端點。兩端主要差異是 V0.5 的編譯期 Ethernet／Wi-Fi 選擇，到 V0.5.2 的單一映像開機自動選擇。現有證據無法將這些差異可靠分配給 V0.5.1，因此沒有產生 V0.5.1 IMG。
