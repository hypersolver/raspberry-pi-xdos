# XDOS UART 指令與開發指南

適用於 XDOS V0.6.2-RC3 XAPP6 HDMI Auto。Pi 3B+ 的除錯埠使用 921600 baud、8N1。使用 3.3 V USB–TTL，轉接器 RX 接 Pi TX（實體 pin 8），TX 接 Pi RX（pin 10），GND 接 GND（例如 pin 6）。Pi 使用自己的電源，轉接器 VCC 留空。

在專案版本目錄安裝 `pyserial`，用 `tools/xdos-uart.py --port COM4 --baud 921600 <子指令>` 操作。把 `COM4` 換成裝置管理員顯示的埠。

| 韌體指令 | Python 子指令 | 用途 |
| --- | --- | --- |
| `PING` | `ping` | 確認韌體在線，回覆 `@XDOS READY 1` |
| `POWER` | `power` | 讀取即時和本次開機以來的欠壓狀態 |
| `LVGL` | `lvgl` | 讀取最近的效能取樣 |
| `OPEN <捷徑>` | `open BENCHMARK` 等 | 從桌面啟動 XAPP。可用 `NOTEPAD`、`CALCULATOR`、`PAINT`、`FILES`、`BROWSER`、`WIDGETS`、`BENCHMARK` |
| `CLOSE` | `close` | 觸發目前 XAPP 的關閉按鈕。視窗關閉動畫完成後可開啟下一個 |
| `BENCH-RESULT` | `bench-result` | 讀取官方 Benchmark 最近一輪的場景 FPS、Render ms、Flush ms，以及測試起訖的刷新週期和 VC4 狀態 |
| `PUT <檔名> <長度> <CRC32>`、`DATA <十六進位資料>`、`END` | `scp` | 經 UART 傳檔到 SD 的 `XDOS/INBOX/` |
| `FETCH <主機 IPv4> <埠> <檔名> <長度> <CRC32>`、`FETCH-STATUS` | `net-scp` | 由 Pi 從區網 HTTP 下載並檢查檔案 |
| `CANCEL` | `scp` 客戶端在失敗時使用 | 取消 UART 分段傳檔 |
| `BOOT <檔名> <CRC32>` | `boot` | 校驗核心檔，更新下次開機的 `config.txt` |
| `BOOT-CLEAR` | `boot-clear` | 還原上一份開機設定 |
| `RESET` | `reset` | 重啟 Pi |

直接使用序列埠時，每行以 `@XDOS ` 開頭並以換行結束，例如 `@XDOS PING`。傳檔協定的長度與 CRC32 使用八位十六進位數，`FETCH` 的 TCP 埠為十進位。一般指令回覆以 `@XDOS OK` 或 `@XDOS ERROR` 結尾，`RESET` 回覆 `@XDOS RESETTING`。Python 客戶端會處理這些格式。區網傳檔詳見 [網路部署指南](NETWORK_UPDATE_ZH.md)。

## 如何新增 UART 指令

1. 在 [UART 服務](tools/rc3-uart/uart_service.h) 的 `Dispatch()` 加入命令比對，將解析和工作拆進獨立函式。建置時 `apply_uart_shortcuts.py`、`apply_uart_net_fetch.py` 和 `build_arduino_demos.py` 會依序加入捷徑、網路與 Benchmark 指令。修改既有命令時，先在這些腳本找出實際產生它的位置。
2. 檢查參數數量、長度、數值範圍及執行條件。需要 SD 的命令檢查 `mounted`。會修改開機設定、啟動 XAPP 或重啟的命令，需避免和正在進行的傳檔重疊。長時間操作可採用 `FETCH` 的排程工作與狀態查詢方式。
3. 保留既有 `@XDOS` 回覆格式。多列資料可參考 `BENCH-RESULT`，用有前綴的資料行、列數和完成回覆，讓客戶端能偵測漏行。
4. 在 [Python 客戶端](tools/xdos-uart.py) 登記子指令與參數，再加上對應的回覆處理。更新上面的指令表，實測正常參數、錯誤參數和傳檔忙碌時的行為。

目前韌體使用 `Dispatch()` 與各建置腳本加入指令，尚未改為單一註冊表。上表是現有指令的開發參考。
