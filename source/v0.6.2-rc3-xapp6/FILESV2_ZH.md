# FILESV2 繁體中文說明

Files 已移植為外部 V2 XAPP（FVFILES.XAPP），可由桌面或 UART 啟動，並與 Notepad 同時執行。目錄每頁上限及單檔刪除檢查仍由核心執行。

## 更新既有 XDOS microSD

1. 下載 FILESV2.IMG、FVFILES.XAPP、FVNOTE.XAPP 和 SHA256SUMS.txt。執行 `sha256sum -c SHA256SUMS.txt` 或用 PowerShell `Get-FileHash -Algorithm SHA256` 核對三個檔案。
2. 把 IMG 放到 FAT32 microSD 根目錄，兩個 XAPP 放到 XDOS/INBOX。備份 config.txt，將 `kernel=FILESV2.IMG` 寫入設定後開機。保留既有桌面捷徑清單。
3. 區網更新使用 921600 baud、8N1 的 UART 傳檔工具。依原檔名用 `net-scp` 傳送兩個 XAPP 和 IMG，再執行 `boot FILESV2.IMG --name FILESV2.IMG` 與 `reset`。環境設定見[基礎安裝及部署指南](OPERATIONS_ZH.md)。
4. 回退時以 BOOT 選擇 FILECLR.IMG 並重開，或還原 microSD 的 config.txt。配對的 CLNOTE.XAPP 保留於 XDOS/INBOX。

## Files 操作

- 單擊檔案可選取。清單為白底，選取列為藍底白字。
- 雙擊 XAPP 可啟動，點擊資料夾可進入。用滾輪或捲軸瀏覽，超過每頁 128 筆時用 Prev 或 Next 翻頁。
- 長按檔案列，再按 Delete 確認刪除該檔案。此操作會拒絕刪除目錄。
- UART 可執行 `OPEN FILES` 與 `OPEN NOTEPAD`。若啟動失敗，確認配對 XAPP 位於 XDOS/INBOX，且通過 SHA-256 核對。

## Pi 3B+ 實測

- 有線區網部署通過 IMG 與兩個 XAPP 的 CRC32。RESET 後以 1920×1080 開機。
- UART 確認 Files 與 Notepad 分別在 V2 槽 0、1 同時執行。關閉 Files 後槽 0 釋放，Notepad 繼續執行。重新開啟 Files 後兩個槽均恢復使用。
- HDMI 確認白底、藍底白字選取、雙擊啟動、滾輪及單檔刪除均正常。

首次啟動曾出現一次 LVGL 樣式初始化警告。原始碼檢查發現，LVGL 過渡樣式使用未清零的記憶體，初始化檢查會讀取舊的樣式標記。Pi 3B+ 診斷版重開後未重現警告，原始呼叫點尚未證實。測後已切回驗收版，兩個程式均可重新開啟。
