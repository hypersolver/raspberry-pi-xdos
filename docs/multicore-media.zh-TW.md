# XDOS V0.6.2-RC3 XAPP6 MultiCore Media · Pi 3B+

[English](multicore-media.md) · [Release 下載](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-multicore-media)

## 變更與研發紀錄

- 以 MultiCore APPSCAN 為基線，保留兩排各五個動態捷徑。桌面略過舊的 `UPDATE.APP`，UART 核心更新功能保留。
- 正式映像使用實測的 `MCMEDNET.IMG`。桌面提供 Terminal、MP4 Player 和 MP3 Player。MP4 使用 VideoCore H.264 硬體解碼，只播放影像。MP3 Player 支援 MP3 和受支援的 PCM WAV。
- Pi 3B+ 已透過 Ethernet 部署，全部檔案通過 CRC32，RESET 後以 1920×1080 開機。實機確認 UPDATE 未顯示、Terminal 由桌面啟動、MP4 畫面會動，MP3 從 HDMI 出聲。
- 320×240 H.264 測試片的 120 幀全部解碼及顯示，UART 回報 `PLAY DONE`。該輪仍有 VCHIQ `WARN_ON`，幀間隔 P95 落在 245 ms 分組。這版已驗證可播放，流暢度仍待改善。3.5 mm 輸出未在本輪重新驗收。

## 首次安裝

1. 核對 `SHA256SUMS.txt`。Windows 用 `Get-FileHash -Algorithm SHA256`，WSL 可用 `sha256sum -c SHA256SUMS.txt`。
2. 將完整 SD ZIP 解壓到已有 Pi 3B+ 開機韌體的 FAT32 分割區。先備份原 `config.txt` 和舊版檔案。保留現有韌體設定，只設一個有效的 `kernel=MCMEDNET.IMG`，確認 `arm_64bit=0`。
3. 開機後點 **Terminal**、**MP4 Player** 或 **MP3 Player**。選檔後在 MP4 Player 按 **PLAY**，在 MP3 Player 按 **HDMI** 或 **3.5 MM**。影片可放 `XDOS/VIDEO/`，音樂可放 `XDOS/MUSIC/`，兩者皆可用 `XDOS/INBOX/`。安裝包未附測試影音。
4. 離開播放器後再開下一個 XAPP。MP3 音訊可按 **STOP** 停止。

[首次建立 XDOS microSD](xdos-quick-start.zh-TW.md) · [桌面圖示規格](xapp-desktop-spec.zh-TW.md)

## Ethernet 更新與回退

以 921600 baud、8N1 的 Debug UART 觸發 `net-scp`，資料由 Ethernet 傳送。先依原檔名上傳六個 `MN*.XAPP`、清單 ZIP 內 `Network/` 的十個 `MNAPP0.APP` 至 `MNAPP9.APP`，最後傳 `MCMEDNET.IMG`。各檔 CRC32 通過後選擇 `boot` 並 `reset`。核心在 FAT32 掛載後補入缺少的清單，並保留同名原有清單。

保留 MCASCAN 核心與配對 XAPP。若切換前正在使用 MCASCAN，可用 `boot-clear` 再 `reset` 回退。也可關機後編輯 `config.txt`，將 `kernel=` 指回 `MCASCAN.IMG`。回退後若仍看到新播放器捷徑，可先備份再移走 `XDOS/APPS/PLAYER.APP` 和 `MUSIC.APP`。 [網路部署指令](../source/v0.6.2-rc3-xapp6/NETWORK_UPDATE_ZH.md)

## 常見問題

| 問題 | 處理方式 |
| --- | --- |
| 沒有播放器或 Terminal | 檢查 `.APP` 是否在 `XDOS/APPS/`、`Exec=` 的檔案是否存在，以及是否超過前十個有效清單。使用 SD ZIP 補齊缺少的檔案後重開。 |
| 桌面先出現，圖示較晚才出現 | 背景先繪製。圖示會等 USB 初始化、SD 卡就緒、FAT32 掛載、清單掃描及執行檔檢查。見下方實測。 |
| MP4 沒有聲音 | MP4 Player 播放影像。獨立音樂請用 MP3 Player。 |
| MP4 播放卡頓或 UART 有 VCHIQ 警告 | 先用低解析度 H.264 測試。記錄片段解析度與 UART `PLAY` 結果。 |
| 3.5 mm 只有單側出聲 | 先改選 HDMI。歷史測試曾觀察到類比單聲道，本次驗收使用 HDMI。 |
| 顯示 XAPP 不相容 | 從同一 Release 重新取得 IMG 和配對 XAPP，核對名稱及雜湊。 |

## 圖示開機速度試驗

在同一張 SD 卡與 Pi 3B+ 上，以 Ethernet 部署並各 RESET 一次。加速版讓 SD 初始化與清單掃描先執行，再延後網路 PHY 探測。`.APP` 的檢查和十個捷徑上限維持原設定。

| 指標 | 對照版 | ICONFAST |
| --- | ---: | ---: |
| 核心開始初始化至桌面背景首繪 | 1.187 秒 | 1.177 秒 |
| 核心開始初始化至圖示首繪 | 5.981 秒 | 5.620 秒 |
| SD 初始化 | 1.536 秒 | 1.536 秒 |
| FAT32 掛載 | 0.864 ms | 0.859 ms |
| 清單掃描與執行檔檢查 | 49.033 ms | 48.962 ms |

圖示首繪提前約 362 ms。UART 已確認 ICONFAST 的 Terminal 可開啟及關閉。時間由核心計時器記錄，代表首次軟體繪製。ICONFAST 是獨立驗證版，正式 Release 仍為實測影音版 MCMEDNET。建置可對生成的 MCMEDNET 原始碼樹執行 `tools/vc4/apply_icon_boot.py SAMPLE --fast --profile`，再編譯及重新配對 XAPP。省略 `--profile` 可停用量測訊息。
