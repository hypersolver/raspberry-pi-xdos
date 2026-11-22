# FVR18 繁體中文說明

桌面底部工具列顯示 CPU0–CPU3 使用率。XAPP 執行時，對應核心欄位會顯示程式名稱。桌面閒置時仍顯示四核百分比。十個 J7 XAPP 與 FVR17 逐位元相同。

## 安裝、操作與回退

1. 下載 `FVR18.IMG`、所需的十個 J7 XAPP 與 `SHA256SUMS.txt`。用 `sha256sum -c SHA256SUMS.txt` 或 PowerShell `Get-FileHash -Algorithm SHA256` 核對。
2. 依[區網與 UART 部署指南](OPERATIONS_ZH.md)以 `net-scp` 傳送 IMG 與缺少的 J7 檔，可沿用已核對的 FVR17 J7 檔。執行 `boot FVR18.IMG --name FVR18.IMG` 後再 `reset`。手動更新時備份 `config.txt`，將 IMG 放至開機分割區根目錄、缺少的 J7 檔放至 `XDOS/INBOX`，並設定 `kernel=FVR18.IMG`。
3. 從底部工具列查看四核使用率。可從桌面啟動 XAPP，或透過 UART 執行 `OPEN NOTEPAD`。程式關閉後，對應名稱會清除。
4. 回退時用 BOOT 和 RESET 選回 `FVR17.IMG`，也可還原 microSD 的 `config.txt`。兩版可共用相同 J7 檔。

## Pi 3B+ 實測

區網部署通過 CRC32，重開後以 1920×1080 顯示。UART 確認閒置時四個 V2 槽空閒，Notepad 與背景 Core Demo 可並行且 CPU3 被借用。關閉兩者後，四槽釋放，CPU3 歸還 LVGL。HDMI 驗收確認閒置、執行與關閉後的四核百分比及程式名稱顯示均正常。
