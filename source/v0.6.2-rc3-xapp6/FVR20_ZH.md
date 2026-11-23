# Raspberry Pi 3B+ FVR20

FVR20 將 GPIO14/15 的除錯序列埠切換到 mini UART，讓出 PL011。電腦端仍用 921600 baud、8N1。韌體將 VideoCore 核心時脈固定為 400 MHz，此時 mini UART 實際約為 925925 baud，與設定值相差 0.47%。本版也加入 `FILE LIST <路徑>`，並在傳送長回覆時等待緩衝區有空間。

## 安裝與回退

先按 SHA256SUMS.txt 校驗 IMG 和 XAPP。FVR20 沿用 FVR19 的十個 J7 XAPP。網路或 microSD 安裝請見[部署指南](OPERATIONS_ZH.md)。網路傳檔前先核對 Pi 現行 IP，再選擇 Windows 綁定位址。備份 config.txt 後部署 FVR20.IMG，用 BOOT 選取並送出 RESET。若需要更新韌體時脈設定，系統會再重開一次。

回退時用 BOOT／RESET 切回 FVR19.IMG，再核對開機標籤和 UART PING。只切換核心時，時脈設定仍會留在 config.txt。需要一併還原時，使用原本的設定備份，確認 kernel 指向 FVR19.IMG 後再重開。若 UART 無法連線，可將 microSD 插到電腦修改開機設定。

## 實機驗證

Pi 3B+ 的 256 KiB UART 傳輸用時 22.8 秒，CRC32 通過。區網以新檔名傳送 256 KiB，FETCH 和 CRC32 皆通過。四處目錄清單完整，觀察 60 秒期間核心時脈維持 400 MHz。回退後確認 FVR19 開機與 UART PING 正常。
