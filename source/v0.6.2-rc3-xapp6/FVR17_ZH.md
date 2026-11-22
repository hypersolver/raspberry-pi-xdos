# FVR17 繁體中文說明

FVR17 在載入外部 V2 XAPP 時解析匯入。J7 模組可在提供所需函式與資料匯出的相容 R17 核心上執行。原有綁定式模組仍需配對核心。UART PUT 和區網 FETCH 遇到同名檔案時，大小與 CRC32 一致即視為成功。大小或 CRC32 不同會拒絕，保留原檔。

## 安裝與操作

1. 下載 `FVR17.IMG`、十個 `J7*.XAPP` 與 `SHA256SUMS.txt`。用 `sha256sum -c SHA256SUMS.txt` 或 PowerShell `Get-FileHash -Algorithm SHA256` 核對。
2. 依[部署指南](OPERATIONS_ZH.md)設定 XDOS UART 工具為 921600 baud、8N1。以 `net-scp` 按發布的檔名傳送 IMG 和 J7 模組，再執行 `boot FVR17.IMG --name FVR17.IMG` 和 `reset`。新版工具收到 PUT 的 `PRESENT` 後即完成，無須再傳資料。
3. 手動更新時，先備份可開機 XDOS FAT32 卡的 `config.txt`。將 IMG 放在根目錄，J7 模組放到 `XDOS/INBOX`，設定 `kernel=FVR17.IMG`。保留原有桌面捷徑。
4. 桌面操作方式沿用既有版本。也可從 UART 執行 `XAPP2 J7NOTE.XAPP WINDOW` 啟動 Notepad。若同名檔的內容不同，請改用新檔名。回退時，以 BOOT 和 RESET 選擇 `FVMNET2.IMG`，或從 microSD 的 `config.txt` 選回該映像。保留 N9 配對模組。

## Pi 3B+ 實測紀錄

- FVR17 核心及十個 J7 模組通過區網 CRC32。兩輪開機分別確認 FVABI1、FVR17 的版本標籤與 UART PING READY。
- 同一份 3,228 B 的 `J7NOTE.XAPP` 在兩個核心上均能啟動、關閉與釋放 V2 槽。切換核心期間沒有重傳。
- 同名同內容的 PUT 回 `PRESENT`，無須重送。相同檔案的 FETCH 也成功。
- 將同名同大小的副本改一個位元後，PUT、FETCH 都拒絕。再次用原檔做 FETCH 仍通過。最後也確認 FVR17 的 Files 捷徑可啟動。
