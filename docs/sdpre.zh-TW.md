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
