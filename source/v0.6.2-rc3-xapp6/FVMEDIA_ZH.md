# FVMEDIA 繁體中文說明

Browser、MP4 Player、MP3 Player、官方 LVGL Widgets 和官方 Benchmark 已移植為外部 V2 XAPP。Files、Notepad、Calculator、Paint 和 Terminal 也已針對本版核心重編。視窗置頂、鍵盤焦點、初始位置與拖曳範圍由核心統一管理。為保留官方 Benchmark 的完整資產，核心配置一個 768 KiB 槽與三個 320 KiB 槽。

## 安裝與操作

1. 下載 `FVMEDIA.IMG`、十個 `V7*.XAPP` 和 `SHA256SUMS.txt`。用 `sha256sum -c SHA256SUMS.txt` 或 PowerShell `Get-FileHash -Algorithm SHA256` 核對檔案。
2. 手動更新時，先備份既有 XDOS FAT32 卡的 `config.txt`，將 IMG 放至根目錄，十個 XAPP 放至 `XDOS/INBOX`，並設定 `kernel=FVMEDIA.IMG`。保留現有桌面捷徑清單。
3. 區網更新時，以 921600 baud、8N1 的 UART 傳檔工具執行 `net-scp`，依原檔名傳送 XAPP 與 IMG，再執行 `boot FVMEDIA.IMG --name FVMEDIA.IMG` 與 `reset`。詳見[部署指南](OPERATIONS_ZH.md)。
4. 點擊視窗可見區域可置頂並取得鍵盤焦點。Browser 支援 HTTP 的 OPEN 與 STOP。MP4 可在 SD 選檔或透過 UART 執行 `PLAY <檔案>`。MP3 Player 支援 MP3 與 44.1 kHz PCM WAV，可選 HDMI 或 3.5 mm。Widgets 與 Benchmark 可用 BACK TO DESKTOP 返回。
5. 應用程式無法啟動時，先確認對應的 V7 檔案和校驗值。若大型槽已被占用，先關閉槽 3 的程式再啟動 Benchmark。回退可用 BOOT 與 RESET 切回 FVAPPS、FVWIND 或 FILESV2，也可還原 `config.txt`。請保留對應的 FA、FW 或 FV 配對檔。

## Pi 3B+ 實測

1920×1080 下，核心與十個 XAPP 都通過區網傳輸 CRC32，RESET 後成功開機。UART 確認五個新版應用均由 V2 啟動。Widgets 和 Benchmark 結束後都能釋放槽 3，Browser 與兩個播放器仍保持執行。使用者實機確認置頂、鍵盤焦點與五個應用均正常。
