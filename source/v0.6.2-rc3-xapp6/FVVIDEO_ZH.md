# Raspberry Pi 3B+ FVVIDEO

FVVIDEO 在初始化解碼器前檢查 GPU 記憶體。若已知配置低於 128 MB，會依畫素總數拒絕超過 640×480 的影片。H.264 解碼器報錯時停止送入新資料。

修改 config.txt 前先備份。要測這次驗證用的 720p 影片，設定 gpu_mem=128 並重開，並為系統保留足夠記憶體。回退時還原原設定並選回保留的核心。請參考[安裝與回退](OPERATIONS_ZH.md)和[影音操作](../../docs/media-playback.md)。

播放支援取決於片源及韌體。1080p 和所有 VideoCore 故障的回復能力尚未確認。

## 實機驗證

Pi 3B+ 在 64 MB 拒播 720p 測試片，UART 保持正常。在 128 MB 重播同一片段，送入、解碼及呈現各 359 幀。播放後與還原 64 MB、切回 FVR19 後均確認 UART PING 正常。
