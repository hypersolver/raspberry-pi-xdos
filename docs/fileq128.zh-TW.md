# FILEQ128 · Raspberry Pi 3B+

本預覽版保留兩排各六格桌面、電源選單、XAPP 工作列與視窗控制。
Arduino Files 支援可捲動的 SD／USB 目錄樹與檔案清單、路徑歷程、
文字預覽及啟動 XAPP。核心目錄服務每頁最多讀取 128 筆，較大的目錄
可使用 Prev 和 Next 翻頁。Arduino Image View 支援未壓縮的 24 位元
及 32 位元 BMP。

## 研發與實測

Pi 3B+ 在區網部署後以 1920×1080 啟動。核心及配對 XAPP 都通過
CRC32，UART 可開啟 Files，之後 PING 仍正常。滑鼠滾輪、大目錄翻頁
和 BMP 畫面仍待 HDMI 確認。

RPiDuino 0.1.4 已用 Windows Arduino CLI 編譯 Basic、LED Blink、
LVGL 點擊計數和 Image View 四個 XAPP，裸機 Blink IMG 也通過編譯。
上傳流程已用模擬接收端測試。IDE 實機上傳、其餘應用的 Arduino
移植及四 XAPP 並行仍待完成。

## 安裝、更新與回退

1. 核對 SHA-256。把 SD 更新包解壓到已有 Pi 3B+ 開機韌體的 FAT32
   分割區。全新卡須先依安裝指南準備開機韌體。
2. 備份 config.txt，把有效的 kernel 設定改成 kernel=FILEQ128.IMG。
   保留前一版 IMG 與配對 XAPP，再開機。
3. 打開 Arduino Files，點目錄進入，點相容的 .XAPP 啟動。
   圖片播放器 Q1IMAGE.XAPP 位於 XDOS/INBOX。
4. 區網更新可經 UART net-scp 暫存映像及配對 XAPP，再執行 BOOT
   和 RESET。安裝桌面清單或替換同名檔時，使用 SD 更新包。
5. 在 Arduino IDE 選 RPiDuino 3/3B+，將 Build target 設為 XDOS XAPP，
   使用套件提供的 FILEQ128 配對描述檔及 debug UART。

XAPP 被拒絕時，確認核心指紋與 XAPP 配對。USB 目錄未就緒時，等
FAT32 USB 掛載後重新整理。桌面最多顯示十二個有效清單。Shutdown
後的 HDMI 斷訊仍待驗證。如需回退，經 UART 選回上一版 IMG，或將
microSD 放回電腦還原原先的 config.txt。

[安裝與操作](../source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) ·
[Arduino 說明](../Arduino%20SDK/XDOS-SAMPLES.md) · [English](fileq128.md)
