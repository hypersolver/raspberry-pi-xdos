# XDOS FILECLR 繁體中文說明

Files 視窗與檔案清單改為白底，選取列為藍底白字。沿用 FILEDBL 的操作。單擊選取，雙擊進入目錄或執行 XAPP。清單超出視窗時可用捲軸或滑鼠滾輪瀏覽，每個目錄最多列出 128 筆。刪除功能限單一檔案。

## 安裝與回退

從 Release 下載 FILECLR.IMG、CLNOTE.XAPP 與 SHA256SUMS.txt。用 sha256sum -c SHA256SUMS.txt 或 PowerShell Get-FileHash -Algorithm SHA256 核對兩個檔案。本版適用既有 XDOS FAT32 microSD。首次安裝請先依[安裝與操作指南](OPERATIONS_ZH.md)建立基本系統。

- microSD 更新。先備份 config.txt。把 FILECLR.IMG 放在卡片根目錄，把 CLNOTE.XAPP 放在 XDOS/INBOX，再將 config.txt 的 kernel= 設為 FILECLR.IMG 並開機。
- 網路更新。使用既有區網與 921600 baud、8N1 的 UART。依操作指南以 net-scp 傳送 CLNOTE.XAPP 和 FILECLR.IMG，保留原檔名。以 boot 指定 FILECLR.IMG，再執行 reset。
- 回退。保留 FILEDBL、FILEDEL、NOTEV2 和 XAPNET 的映像與配對 XAPP。以 boot 選擇回退映像後 reset。若 UART 無法使用，可在電腦上修改 microSD 的 config.txt。
- Notepad 若無法開啟，確認 CLNOTE.XAPP 位於 XDOS/INBOX，並核對 SHA-256。

## Pi 3B+ 實機驗證

- 透過有線網路部署並重開，UART 回報 FILECLR 版本，且確認 OPEN FILES 成功開啟。
- HDMI 實機確認視窗白底、選取列藍底白字。
- 單擊選取、雙擊啟動、滑鼠滾輪與刪除已在前一版 FILEDBL 通過。本版沿用。
