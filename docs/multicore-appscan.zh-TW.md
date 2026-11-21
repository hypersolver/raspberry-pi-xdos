# XDOS V0.6.2-RC3 XAPP6 MultiCore APPSCAN · Pi 3B+

## 變更與研發紀錄

- 合併 MultiCore 的三個 LVGL 繪圖工作核心、有線 Ethernet、VC4 EGL 和 `HDMI-MODE`，與 APPSCAN 的動態桌面。
- SD 掛載後依 FAT 目錄順序收錄前 10 個有效 `.APP`。桌面固定兩排各五格。圖示為名稱的第一個英文字母，A–Z 各有底色。名稱置中，超過 16 個顯示字元時隱藏整行，圖示仍可點擊。
- QA3 在 Pi 3B+ 1080p 實測，11 份清單載入 10 個。HDMI 確認兩排、16 字元名稱置中，以及 17 字元名稱隱藏後仍能開啟。正式包移除 QA 測試清單，改掃描 `XDOS/APPS/`。
- 正式映像 `MCASCAN.IMG` 與四個配對 XAPP 已從 Ethernet 部署並通過 CRC32。Pi 3B+ RESET 後確認 1920×1080，UART 逐一開啟並關閉 Widgets、Benchmark、Browser 和 Terminal。
- 先前 MultiCore 的 Benchmark 數字屬於原基線。 [基線測量紀錄](../source/v0.6.2-rc3-xapp6/MULTICORE_ZH.md)

## 首次安裝

[正式版下載](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-multicore-appscan) · [建置來源](https://github.com/hypersolver/raspberry-pi-xdos/releases/download/v0.6.2-rc3-xapp6-multicore-appscan/XDOS-MultiCore-APPSCAN-source.zip)

1. 核對 Release 的 `SHA256SUMS.txt`。Windows 可用 `Get-FileHash -Algorithm SHA256 <檔案>`，Linux／WSL 可用 `sha256sum -c SHA256SUMS.txt`。
2. 準備已有 Pi 3B+ 開機韌體的 FAT32 microSD。保留前版卡或備份整張卡與 `config.txt`。將 SD ZIP 解壓到 FAT32 開機分割區根目錄。
3. 保留原有韌體設定，在通用區段使用單一 `kernel=MCASCAN.IMG` 並確認 `arm_64bit=0`。UART 使用 921600 baud、8N1。安全退出、插回 Pi、接 HDMI 和 USB 鍵鼠再開機。
4. 點應用圖示或名稱開啟。視窗按 **X** 關閉，LVGL Demo／Benchmark 按 **BACK TO DESKTOP**。

[首次安裝詳解](xdos-quick-start.zh-TW.md) · [桌面規格](xapp-desktop-spec.zh-TW.md)

## Ethernet 更新與回退

既有 XDOS 能用 UART 觸發 `net-scp`，資料走 Ethernet。先傳下列配對模組及 `MCASCAN.IMG`，確認每檔 CRC32，再執行 `boot` 並 `reset`。若原卡尚缺 `.APP` 或其 `Exec=` 目標檔，先使用 SD ZIP 補齊 `XDOS/APPS/`。網路工具的目的地是 `XDOS/INBOX/`。

| 來源 | Pi 端目標 |
| --- | --- |
| `LVGLDEMO.XAPP` | `ASWIDGET.XAPP` |
| `LVGLBENCH.XAPP` | `ASBENCH.XAPP` |
| `BROWSER.XAPP` | `ASBROWSE.XAPP` |
| `TERMINAL.XAPP` | `ASTERM.XAPP` |

保留舊核心及其配對應用。需要回退時可用 `boot-clear` 和 `reset`。若 UART 不可用，關機，在電腦上還原 `config.txt` 的舊核心名稱。 [UART 指令](../source/v0.6.2-rc3-xapp6/UART_ZH.md) · [區網部署](../source/v0.6.2-rc3-xapp6/NETWORK_UPDATE_ZH.md)

## 常見問題

| 問題 | 操作 |
| --- | --- |
| 應用未出現在桌面 | 核對 `XDOS/APPS/` 第一層的 `.APP` 及其 `Exec=` 路徑。重開機後重新掃描。超過 10 個時，後面的項目留在卡上。 |
| 圖示有顯示，但名稱空白 | 名稱超過 16 個顯示字元會隱藏。縮短 `Name=` 後重開機。 |
| XAPP 顯示不相容 | 使用同一 Release 的核心與配對 XAPP，並核對檔名與 SHA-256。 |
| Ethernet 傳檔失敗 | 確認 Pi 和電腦位於同一區網、UART 921600、主機防火牆允許私人網路連線，且 INBOX 沒有同名檔。 |
| 開機黑屏 | 先確認 FAT32、HDMI、`kernel=` 指向正確 IMG。能連 UART 時用 `ping`，必要時回退到原卡。 |

## 發行範圍

本包以 Pi 3B+ 的 MultiCore 有線核心為基礎。Arduino IDE 一鍵編譯並安裝新 XAPP、獨立 LVGL 影音範例實機驗收、exFAT／NTFS，以及 Pi 4／5 的 XDOS XAPP 執行環境另外追蹤。使用 MP4 播放器時可回到 PXLAYOUT；影音測試版的資料見 [XAPP 開發指南](arduino-xapp.zh-TW.md)。
