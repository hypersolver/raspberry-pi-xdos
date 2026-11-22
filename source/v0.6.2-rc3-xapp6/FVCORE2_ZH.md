# FVCORE2 繁體中文說明

FVCORE2 在 FVMNET2 上整合 R16-B 排程策略。桌面可解析 `Screen=Full|Window|None`，V1 視為全螢幕，V2 未指定時預設 Window。第二個全螢幕 XAPP 會收到 `-11`。沒有全螢幕程式時最多借用三顆核心，執行全螢幕程式時最多借兩顆，保留一顆給 LVGL。CPU0 工作連續三段超過 10 ms 會降頻至每四個 tick 執行一次，並顯示提示。借用核心可使用 `xdos_core_time_us()` 與 `xdos_core_sleep_us()`。

## 安裝、操作與回退

本版為 Pi 3B+ 驗證版。附件包含實測核心、配對 Files 和 Notepad、Core Demo、超時與睡眠測試程式。FVMNET2 的其他桌面應用不在這套實測包內。若要回到完整桌面，請保留 FVMNET2 與 N9 配對檔。

1. 下載 `FVCORE2.IMG`、五個 FK XAPP 與 `SHA256SUMS.txt`。以 `sha256sum -c SHA256SUMS.txt` 或 PowerShell `Get-FileHash -Algorithm SHA256` 核對。
2. 依[部署指南](OPERATIONS_ZH.md)設定 UART 921600 baud、8N1，以 `net-scp` 按原檔名上傳 IMG 與五個 XAPP，再執行 `boot FVCORE2.IMG --name FVCORE2.IMG` 和 `reset`。手動更新可先備份 `config.txt`，將 IMG 放在 FAT32 根目錄、五個 XAPP 放入 `XDOS/INBOX`，再於 `config.txt` 選擇映像。
3. UART 可用 `XAPP2 FKCORE.XAPP NONE`、`XAPP2 FKNOTE.XAPP FULL`、`XAPP2 FKSLOW.XAPP` 或 `XAPP2 FKSLEEP.XAPP`。用 `XAPP2-STATUS` 查槽位和借用核心，用 `XAPP2-CLOSE <槽號>` 關閉。
4. 回退時用 BOOT 選回 `FVMNET2.IMG`，RESET 後核對開機標籤與 UART PING READY。若 UART 無法回應，可還原 microSD 的 `config.txt`。

## R16-B 實測與回退

所有附件均通過 Pi 3B+ 區網傳檔 CRC32，RESET 後成功啟動 FVCORE2。測試批次結束碼為 0。

| 項目 | Pi 3B+ 實測 |
| --- | --- |
| 四個無視窗 XAPP | 槽 0–3 全部執行，三顆核心出借，第四個 XAPP 在 CPU0 分時。關閉後槽位全數釋放。 |
| 全螢幕互斥 | 一個全螢幕和三個視窗版並行，借兩顆核心，CPU1 留給 LVGL。第二個全螢幕請求回 `-11`。 |
| CPU0 超時降頻 | 約 15 ms 的測試切片觸發降頻提示與 `app/throttled`。 |
| 借核心睡眠 | CPU3 執行 20 次 100 ms 睡眠，共 2,001 ms，單次最大超出 125 µs。 |
| 提前停止 | CLOSE 在第二輪睡眠完成前停止工作，槽位釋放。 |
| 回退 | BOOT、RESET 回到 FVMNET2，核對開機標籤與 UART PING READY，COM3 已釋放。 |

Arduino V2 的 delay 與產生 `Screen=` 清單欄位仍待整合。
