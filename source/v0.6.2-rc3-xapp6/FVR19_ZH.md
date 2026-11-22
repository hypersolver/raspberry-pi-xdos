# Raspberry Pi 3B+ FVR19

FVR19 在 FVR18 加入開機模式，沿用相同的十個 J7 XAPP。下載後請用 `SHA256SUMS.txt` 核對，卡上相符的 J7 檔可直接沿用。

## 安裝與設定

請參考[部署與操作說明](OPERATIONS_ZH.md)。網路部署時先傳送 `FVR19.IMG`，執行 `boot FVR19.IMG --name FVR19.IMG` 後再執行 `reset`。手動安裝時將映像複製到 microSD 開機分割區根目錄，並在 `config.txt` 設定 `kernel=FVR19.IMG`。

設定檔放在 `XDOS/XDOS.CFG`。主設定檔不存在時，系統會讀取 `XDOS/INBOX/XDOS.CFG`。測試前請備份兩處。

- `MODE=AUTO` 在未設定 `KIOSK` 時進桌面，有設定時進 KIOSK。本版尚未支援依 HDMI 自動選 IOT，無螢幕用途請明確設定 `MODE=IOT`。
- `MODE=DESKTOP` 進入桌面。
- `MODE=KIOSK` 全螢幕啟動 `KIOSK=<檔案.XAPP>`。`KIOSK_EXIT=RESTART|DESKTOP|REBOOT` 決定 XAPP 結束後重新執行、回桌面或重開機。
- `MODE=IOT` 暫停 LVGL 更新，只允許啟動 `Screen=None` 的 XAPP。
- `AUTORUN=<檔案1.XAPP>,<檔案2.XAPP>` 可在各模式啟動背景程式。

UART 的 `@XDOS MODE` 可查詢模式。`@XDOS MODE DESKTOP` 和 `@XDOS MODE KIOSK` 可在執行時切換。進出 IOT 請修改 microSD 設定並重開。

要回退時，還原原設定後切回 `FVR18.IMG` 並重開。原有 J7 XAPP 可繼續使用。

## 常見問題

- 設定沒有生效時，先確認主設定檔。主設定檔的優先順序高於 INBOX 副本。
- BOOT 拒絕映像時，先核對下載檔的校驗碼，並檢查卡上是否已有同名映像。
- KIOSK 程式若連續三次無法啟動，XDOS 會回桌面。請核對檔名並確認程式支援全螢幕。

## 實機回歸紀錄

Pi 3B+ 在 1920×1080 下完成區網傳輸與 CRC32 校驗。UART 驗證開機與執行中模式切換、桌面與 KIOSK／IOT 的 AUTORUN、三種 KIOSK 退出行為，以及 IOT 拒絕視窗 XAPP。測後確認兩處設定回到原狀，切回 FVR18 並收到 UART READY。本輪未做 HDMI 目視驗收。
