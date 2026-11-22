# XDOS FVMNET2 繁體中文說明

FVMNET2 將有線網路自適應輪詢整合進 FVMEDIA。封包活動後的 20 ms 內以 1 ms 間隔輪詢，閒置時改為 10 ms。FETCH 下載期間全程維持 1 ms，結束後恢復自適應。沿用 FVMEDIA 的桌面與應用，核心配對十個 N9 XAPP。

## 安裝、操作與回退

1. 下載 `FVMNET2.IMG`、十個 `N9*.XAPP` 和 `SHA256SUMS.txt`。使用 `sha256sum -c SHA256SUMS.txt` 或 PowerShell `Get-FileHash -Algorithm SHA256` 核對檔案。
2. 區網更新使用 921600 baud、8N1 的 XDOS UART 工具。依原檔名以 `net-scp` 傳送各 XAPP 與 IMG，再執行 `boot FVMNET2.IMG --name FVMNET2.IMG` 和 `reset`。詳見[部署指南](OPERATIONS_ZH.md)。
3. 手動更新時，先備份既有 XDOS FAT32 卡的 `config.txt`。將 IMG 放在根目錄、十個 XAPP 放在 `XDOS/INBOX`，設定 `kernel=FVMNET2.IMG` 後開機。保留現有的桌面捷徑。
4. 桌面應用操作見 [FVMEDIA 指南](FVMEDIA_ZH.md)。UART 的 `[PROF]` 顯示 CPU0 使用率，`[NET]` 顯示輪詢資料。應用無法啟動時，先確認其 N9 配對檔與校驗值。
5. 回退可用 BOOT 選回 `FVMEDIA.IMG` 後 RESET。若 UART 無法使用，可在電腦上修改 microSD 的 `config.txt`。請保留 FVMEDIA 的 V7 配對檔。

## Pi 3B+ 實測

桌面閒置各取五個約 5 秒樣本。CPU0 使用率從 FVMEDIA 的 12.1% 降到 FVMNET2 的 2.0%，FVMNET2 閒置時每秒輪詢 117–121 次。

| 有線 FETCH，同一份 1,592,512 B 檔案 | 時間 | 端到端速度 |
| --- | ---: | ---: |
| FVMEDIA 基準 | 3.824 s | 406.7 KiB/s |
| FVMNET2 第 1 趟 | 5.780 s | 269.1 KiB/s |
| FVMNET2 第 2 趟 | 3.861 s | 402.8 KiB/s |
| FVMNET2 第 3 趟 | 3.707 s | 419.5 KiB/s |

首版整合曾兩次 FETCH 失敗。FETCH 改為維持 1 ms 後，三趟均通過 CRC32。第一趟慢於基準，後兩趟接近基準。核心和十個 XAPP 的部署 CRC32 均通過，FVMNET2 本身的 BOOT、RESET 和重開後 UART PING READY 已驗證，V2 Files 也成功開啟。
