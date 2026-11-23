# Raspberry Pi 3B+ FVR21

FVR21 在 FVR20 加入唯讀 UART 指令 `@XDOS NET`，回報網路就緒狀態、DHCP 或靜態模式、IPv4 位址、子網路遮罩和預設閘道。查詢不會修改設定。

區網部署失敗時，用 921600 baud、8N1 連上 UART 並執行 `@XDOS NET`。若回報 `up=0` 或 `ip=0.0.0.0`，檢查網路線並等候取址。重新部署時，在 `net-scp --bind` 指定 Pi 能連到的電腦位址。若 Pi 仍使用舊網段的靜態 IP，先檢查並備份 `NET.CFG`，再修改設定。

安裝與回退請依[部署指南](OPERATIONS_ZH.md)，並用 SHA256SUMS.txt 核對核心與十個 J7 XAPP。

## 實機驗證

Pi 3B+ 通過區網傳檔與 CRC32、BOOT／RESET、UART DHCP 狀態查詢及主機網卡位址選擇。回退 FVR19 並確認 UART PING 正常。靜態設定和拔除網路線的情境未做實機驗證。
