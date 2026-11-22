# XDOS 研發與驗證紀錄

## 已公開的實機基準

| 階段 | 結果 |
| --- | --- |
| 128 列 LVGL 緩衝 | Pi 3B+ 的 1080p 捲動測試完成，透過 EGL VSync 呈現。保留既有回退映像 |
| PXBOOT | 深藍／青綠的 Pixel BBS 開機畫面與桌面共用程式化圖樣。進度依同步啟動階段更新，高解析度放大桌面標題與捷徑字體 |
| PXLAYOUT | 2026-11-21 在 Pi 3B+ 驗證 720p、1080p。每種解析度的九個圖示與九個名稱均可用滑鼠開啟對應 XAPP |

[PXLAYOUT Release](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-pxlayout) · [安裝、部署、實測與操作](PXLAYOUT_ZH.md) · [原始碼與建置](PXLAYOUT_SOURCE.md)

MultiCore 為獨立效能預發行版。套件內 Pi 3B+ 的 1080p60 UART 紀錄顯示，官方 Benchmark 平均 17 → 46 FPS，捲動場景 10 → 44 FPS。[功能、安裝與原始數據](MULTICORE_ZH.md)

## 本機實驗映像

以下 IMG 是 Pi 3B+ 的開發測試紀錄。公開下載仍使用 PXLAYOUT。

| 功能 | 實測與目前限制 |
| --- | --- |
| 720p H.264 風景片 | 30 FPS、359 幀全部呈現。一次讀入每個 MP4 樣本並提前上傳紋理後，幀間隔 P95 約 85 ms，最長約 165.8 ms。EGL 換頁呼叫 P95 約 2 ms。解碼層以全 I 幀用的送入順序 FIFO 標記 B 幀影片，造成 166 次輸出時間戳未遞增。排程修正待實測 |
| 1080p H.264 | I420 緩衝對齊 1920×1088，顯示裁為 1920×1080。兩次實機播放都收到 VideoCore 資源錯誤，沒有顯示第一幀 |
| Music XAPP | 原始 44.1 kHz MP3 在 HDMI 與 3.5 mm 都已播至結尾。支援 44.1 kHz、16-bit PCM WAV。ARM 軟體解碼 MP3，音訊由 VideoCore 或直接 PWM 輸出。錄音需外接輸入裝置，目前尚未實作 |
| 3.5 mm 雙聲道排查 | 同一副耳機在 Windows 左右正常。Pi 的 HDMI 可播兩段獨立聲道。Pi 類比仍有單耳問題。AUDIOCHK 透過 UART 讀到 PWM_CTL=0x0000a1a1、PWM_STA=0x00000600、DMAC=0x80000707，GPIO40／41 都是 ALT0。DMA 資料的兩個交錯位置各有 87,360 個獨立有聲影格。數位端已分別送出兩路，插孔端的實際類比波形、接觸狀況和故障原因仍未量測 |

### Music 實驗版操作

將 MP3 或 WAV 放入 SD 根目錄、XDOS/INBOX 或 XDOS/MUSIC。桌面開啟 Music，選檔後按 HDMI 或 3.5 MM，按 STOP 停止。UART 可用 OPEN MUSIC 開啟，AUDIOCHK 另可輸入 AUDIO 讀取 PWM 暫存器與 FIFO 取樣摘要。公開 PXLAYOUT 的 UART 指令見[指令指南](UART_ZH.md)。

## FILECLR

Pi 3B+ 已驗證開機、UART 開啟 Files 和 HDMI 配色。詳見[變更、安裝與實測](FILECLR_ZH.md)。

## FILESV2

Pi 3B+ 已驗證外部 Files V2 與 Notepad 並行。詳見[變更、操作及實測](FILESV2_ZH.md)。

## FVMEDIA

[變更、操作與 Pi 3B+ 實測紀錄](FVMEDIA_ZH.md)。

## FVMNET2

[自適應有線網路變更、操作與 Pi 3B+ 實測](FVMNET2_ZH.md)。

## FVCORE2

[R16-B 排程、操作與 Pi 3B+ 實測及回退](FVCORE2_ZH.md)。

## FVR17

[動態匯入、同檔上傳與 Pi 3B+ 實測](FVR17_ZH.md)。
