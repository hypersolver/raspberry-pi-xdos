# XDOS SDPRE · Raspberry Pi 3B+

[English](sdpre.md) · [Release 下載](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-sdpre)

## 變更與實測

SDPRE 將 SD 卡忙碌時的 ACMD41 查詢間隔從 500 ms 縮至 20 ms，並在 USB 枚舉前繪製桌面圖示。SD 和 USB 初始化仍由主核心執行。

| 核心量測項目 | ICONFAST | SDPRE |
| --- | ---: | ---: |
| SD 初始化 | 1.536 秒 | 0.096 秒 |
| 圖示首繪 | 5.620 秒 | 1.478 秒 |
| USB 就緒 | 3.886 秒 | 4.186 秒 |

同一台 Pi 3B+、同一張 microSD、1080p 下各 RESET 一次。核心的軟體繪製計時顯示 **圖示提早 4.142 秒，USB 晚約 300 ms**。UART 確認 Terminal、Browser、Widgets 和 Benchmark 可開啟與關閉。實體滑鼠可點開 Terminal，實體鍵盤可輸入並執行 `HELP`。

## 安裝與操作

1. 依 `SHA256SUMS.txt` 核對下載檔。備份原卡與 `config.txt`，將完整 SD ZIP 解壓到已有 Pi 3B+ 開機韌體的 FAT32 分割區。
2. 保留既有韌體設定，只設一個生效的 `kernel=SDPRE.IMG`，並確認 `arm_64bit=0`。安全退出卡片後，接好 HDMI 與 USB 鍵鼠再開機。
3. 點桌面圖示或名稱啟動。完整包內含 `.APP` 與執行檔。核心綁定模組為 `SPWIDGET.XAPP`、`SPBENCH.XAPP`、`SPBROWSE.XAPP` 和 `SPTERM.XAPP`。MP4／MP3 模組沿用 MCMEDIA 的非核心綁定檔 `MNMP4.XAPP`、`MNMP3.XAPP`。

## Ethernet 更新與回退

現有 MCMEDIA 卡可由 921600 baud、8N1 的 Debug UART 觸發 `net-scp`。以原檔名傳送四個 `SP*.XAPP` 與 `SDPRE.IMG`，逐檔確認 CRC32，再執行 `boot SDPRE.IMG --name SDPRE.IMG` 與 `reset`。保留卡上原有的 MP4／MP3 模組與清單。新卡或缺少應用時請使用完整 SD ZIP。

保留 `ICONFAST.IMG` 及其 `IF*.XAPP`。若切換前使用 ICONFAST，可用 `boot-clear` 後 `reset` 回退。也可關機後將 `config.txt` 指回 `kernel=ICONFAST.IMG`。 [UART 指令](../source/v0.6.2-rc3-xapp6/UART_ZH.md)

## 常見問題

- **圖示已出現，滑鼠還不能動**：等 USB 枚舉完成。本次量到圖示首繪至 USB 就緒約 2.7 秒。
- **少了圖示**：檢查 `XDOS/APPS/` 的清單、執行檔路徑及十個捷徑上限。清單變更後重開機。
- **XAPP 不相容**：核對同版核心、配對 XAPP 檔名及 SHA-256。
- **MP4 卡頓或出現 VCHIQ 警告**：先前 MCMEDIA 實測可播放，但仍有警告。本輪 SDPRE 驗收未重新量測影音流暢度。 [MCMEDIA 影音紀錄](multicore-media.zh-TW.md)

## USB 與 SD 交錯初始化試驗

SDPRE 約 2.708 秒的 USB 初始化包含主控啟動、Root Port 等待、Hub 供電穩定、各連接埠重置和裝置枚舉。USBOV2 的 UART 記錄到固定等待共 1,590 ms，包括 Root Port 170 ms、兩次 Hub 供電等待合計 1,020 ms，以及四次 Hub 連接埠重置後等待合計 400 ms。其餘時間未逐項分解。

第一次交錯試驗在背景任務呼叫 VC4 繪圖時停止。經 SD 卡還原 SDPRE 後，USBOV2 只在背景準備 SD 與清單，繪圖回到主工作。在同一台 Pi 3B+ 上各次 RESET 的核心計時如下。

| 指標 | SDPRE | USBOV2 |
| --- | ---: | ---: |
| 圖示首繪 | 1.478 秒 | 2.297 秒 |
| USB 就緒 | 4.186 秒 | 4.316 秒 |
| 滑鼠掛接 | 4.187 秒 | 4.317 秒 |

USBOV2 約 4.316 秒也掛接了鍵盤。UART 確認 Terminal、Browser、Widgets、Benchmark 都可開關。因圖示首繪與 USB 就緒均較晚，PWRMENU 版沿用 SDPRE 的初始化順序。

## PWRMENU 電源選單

在 PWRMENU 試驗版，點桌面左下角 **XDOS** 打開向上彈出的選單。可點 **Restart** 或 **Shut Down**，也可在選單開啟時按底線對應的 **R** 或 **S**。按 Esc 或點選單外可關閉。寫入 SD 或接收更新時會拒絕電源操作。Pi 3B+ 的 **Shut Down** 會停止系統，板子仍由外部電源供電。

PWRMENU 已透過 Ethernet 部署並 RESET。該次圖示首繪為 1.494 秒，USB 就緒為 4.448 秒。UART 確認鍵鼠掛接及 Terminal 開關。
