# XDOS UART 指令與開發指南

指令表以目前公開的 XDOS V0.6.2-RC3 XAPP6 PXLAYOUT 為準。Pi 3B+ 的除錯埠使用 921600 baud、8N1。使用 3.3 V USB–TTL，轉接器 RX 接 Pi TX（實體 pin 8），TX 接 Pi RX（pin 10），GND 接 GND（例如 pin 6）。Pi 使用自己的電源，轉接器 VCC 留空。

在專案版本目錄安裝 `pyserial`，用 `tools/xdos-uart.py --port COM4 --baud 921600 <子指令>` 操作。把 `COM4` 換成裝置管理員顯示的埠。

| 韌體指令 | Python 子指令 | 用途 |
| --- | --- | --- |
| HELP | 直接輸入 @XDOS HELP | 列出本版命令 |
| `PING` | `ping` | 確認韌體在線，回覆 `@XDOS READY 1` |
| `POWER` | `power` | 讀取即時和本次開機以來的欠壓狀態 |
| `LVGL` | `lvgl` | 讀取最近的效能取樣 |
| OPEN <捷徑> | open BENCHMARK 等 | 可用 NOTEPAD、CALCULATOR、PAINT、FILES、BROWSER、TERMINAL、WIDGETS、BENCHMARK、PLAYER |
| PLAY <檔案> | play DEMO.MP4 | 從 SD 播 H.264 MP4。相對檔名由 XDOS/INBOX 查找，可用 CLOSE 停止 |
| `CLOSE` | `close` | 關閉全螢幕 XAPP。FVCLOSE 起也可關閉目前取得焦點的 V2 視窗。使用 XAPP2-CLOSE <slot> 可指定槽位 |
| `BENCH-RESULT` | `bench-result` | 讀取官方 Benchmark 最近一輪的場景 FPS、Render ms、Flush ms，以及測試起訖的刷新週期和 VC4 狀態 |
| `PUT <檔名> <長度> <CRC32>`、`DATA <十六進位資料>`、`END` | `scp` | 經 UART 傳檔到 SD 的 `XDOS/INBOX/` |
| `FETCH <主機 IPv4> <埠> <檔名> <長度> <CRC32>`、`FETCH-STATUS` | `net-scp` | 由 Pi 從區網 HTTP 下載並檢查檔案 |
| `CANCEL` | `scp` 客戶端在失敗時使用 | 取消 UART 分段傳檔 |
| `BOOT <檔名> <CRC32>` | `boot` | 校驗核心檔，更新下次開機的 `config.txt` |
| `BOOT-CLEAR` | `boot-clear` | 還原上一份開機設定 |
| `RESET` | `reset` | 重啟 Pi |

直接使用序列埠時，每行以 `@XDOS ` 開頭並以換行結束，例如 `@XDOS PING`。傳檔協定的長度與 CRC32 使用八位十六進位數，`FETCH` 的 TCP 埠為十進位。一般指令回覆以 `@XDOS OK` 或 `@XDOS ERROR` 結尾，`RESET` 回覆 `@XDOS RESETTING`。Python 客戶端會處理這些格式。區網傳檔詳見 [網路部署指南](NETWORK_UPDATE_ZH.md)。

## 如何新增 UART 指令

1. 終端機整合腳本 apply_terminal.py 產生單一 commandTable_ 和 RunQuery。新增命令時先登記名稱、參數與 Terminal 權限。查詢放 RunQuery，執行型命令放 Dispatch。可參考 [PLAY 的加入方式](tools/vc4/apply_player.py)。
2. 檢查參數數量、長度、數值範圍及執行條件。需要 SD 的命令檢查 `mounted`。會修改開機設定、啟動 XAPP 或重啟的命令，需避免和正在進行的傳檔重疊。長時間操作可採用 `FETCH` 的排程工作與狀態查詢方式。
3. 保留既有 `@XDOS` 回覆格式。多列資料可參考 `BENCH-RESULT`，用有前綴的資料行、列數和完成回覆，讓客戶端能偵測漏行。
4. 在 [Python 客戶端](tools/xdos-uart.py) 登記子指令與參數，再加上對應的回覆處理。更新上面的指令表，實測正常參數、錯誤參數和傳檔忙碌時的行為。

實機支援項目以 HELP 為準。Music 與 AUDIO 測試指令見[研發紀錄](DEVELOPMENT_ZH.md)。
