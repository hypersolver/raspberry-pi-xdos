# Terminal Window · Pi 3B+

從 XDOS 桌面點 Terminal，或由 UART 送 `OPEN TERMINAL`。輸入 `HELP`、`POWER`、`LVGL` 或 `BENCH-RESULT` 後按 Enter。可用視窗右上角的 **X** 關閉。Terminal 視窗沿用縮放動畫與桌面 CPU 百分比。

UART 與 Terminal 使用同一個核心指令表。新增核心指令時，在 `tools/rc3-uart/apply_terminal.py` 的 `commands` 登記名稱、是否有參數與是否開放 Terminal 執行，再在對應的核心處理函式實作。UART `HELP` 會由這張表列出指令。傳檔、開機切換與重啟仍由 UART 操作。

首次安裝可將 SD 測試包內容複製到已有 Pi 開機韌體的 FAT 分割區，備份 `config.txt` 後指定 `kernel=TERMWIN.IMG`。區網更新依 [區網部署指南](NETWORK_UPDATE_ZH.md) 執行，使用下表的配對檔名。

| 本機檔案 | 區網目的檔名 |
| --- | --- |
| TERMINAL.XAPP | TMTERM.XAPP |
| BROWSER.XAPP | TMBROWSE.XAPP |
| LVGLDEMO.XAPP | TMWIDGET.XAPP |
| LVGLBENCH.XAPP | TMBENCH.XAPP |
| TERMWIN.IMG | TERMWIN.IMG |

所有檔案通過 CRC32 後，執行 `boot TERMWIN.IMG --name TERMWIN.IMG`，再執行 `reset`。回退使用 `boot-clear` 和 `reset`。UART 使用 921600 baud、8N1。

Pi 3B+ 已確認桌面與 UART 都能開啟 Terminal，指令輸出、CPU 百分比和關閉動畫正常。`BOOT` 與 `RESET` 通過，`FETCH` 初次重測失敗，重啟後重測通過 CRC32。

在 WSL 版本目錄編譯：

```bash
XDOS_TERMINAL_TEST=1 XDOS_WIRED_ONLY=1 XDOS_ANALYTICS_MODE=analyticsregion XDOS_BROWSER_XAPP=1 XDOS_TEST_DEBUG=0 bash tools/build-wsl.sh "$PWD"
```
