# Raspberry Pi 3B+ FVCLOSE

以 FVR21 為基線。UART CLOSE 優先關閉目前的全螢幕程式，在桌面也可關閉取得焦點的 V2 視窗。要關閉指定槽位時，使用 XAPP2-CLOSE。

部署 FVCLOSE.IMG 前先核對 SHA-256，步驟見[操作指南](OPERATIONS_ZH.md)。J7 模組與 FVR21 相容。透過 UART 執行 OPEN CALCULATOR 或 OPEN BENCHMARK，接著用 CLOSE 關閉，再用 XAPP2-STATUS 核對槽位。回退時用 BOOT 選取保留映像，送出 RESET 後用 PING 確認。

Pi 3B+ 區網 CRC32 通過。Calculator 視窗及全螢幕 Benchmark 都能透過 CLOSE 關閉並釋放槽位，回退 FVR19 後 UART READY。
