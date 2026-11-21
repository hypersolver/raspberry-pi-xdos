# XDOS V0.6.2-RC3 XAPP6 · Pixel BBS Layout

適用 Raspberry Pi 3B+、ARM32。核心 PXLAYOUT.IMG 為 1,491,628 bytes（約 1.42 MiB）。

- 開機與桌面共用程式產生的 8×8 Pixel BBS 顆粒及像素標誌。進度條依已完成的同步開機階段更新。
- 桌面按解析度調整圖示、字體、間距和點擊範圍。較矮畫面可將捷徑分欄。
- 保留 128 列 LVGL 緩衝、VC4 EGL VSync、VSyncOpt 回退及核心區網更新命令。
- MP4 Player 透過 VideoCore 硬體解碼本機 H.264 畫面；此版沒有音訊與 YouTube 串流。影片寬度須為 32 的倍數，高度須為 16 的倍數。

## Pi 3B+ 實測

同一份 IMG 使用 UART 確認 HDMI、XDOS、LVGL 和 VC4 尺寸一致。實體 HDMI 上的圖示、文字與間距皆正常。每輪用實體 USB 滑鼠分別點擊九個圖示與九個名稱，合計 18 次。

| HDMI | 圖示 / 垂直列距 | 點圖示 | 點名稱 |
| --- | --- | --- | --- |
| 1280×720 | 31×31 / 61 px | 9/9 | 9/9 |
| 1920×1080 | 47×47 / 100 px | 9/9 | 9/9 |

九個捷徑為 Terminal、Notepad、Calculator、Paint、Files、Browser、LVGL Demo、官方 LVGL Benchmark 和 MP4 Player。本紀錄驗證啟動與桌面配置。播放器流暢度及其他解析度的 HDMI 外觀未列為本次通過項目。

## 下載與校驗

下載 Release 的 PXLAYOUT.IMG、PLAYER.XAPP、Q9WIDGET.XAPP、Q9BENCH.XAPP、Q9BROWSE.XAPP、Q9TERM.XAPP 及 SHA256SUMS.txt。在 Linux／WSL 於下載資料夾執行：

~~~sh
sha256sum -c SHA256SUMS.txt
~~~

Windows 可用 PowerShell 的 Get-FileHash -Algorithm SHA256，再與校驗檔比對。Q9 系列 XAPP 與此 IMG 同次建置。

[相應原始碼、版本和建置方式](PXLAYOUT_SOURCE.md)

## 首次安裝到 microSD

1. 依[安裝指南](OPERATIONS_ZH.md)準備具相容 Raspberry Pi 開機韌體與 XDOS 基本檔的 FAT32 microSD。備份原 config.txt 和原核心。PXLAYOUT.IMG 是單一裸機核心，直接複製至開機分割區根目錄。
2. 在 config.txt 的適用區段保留既有韌體與 UART 設定，確認 arm_64bit=0，將 kernel 設為 PXLAYOUT.IMG。
3. 把五個 Release XAPP 按原檔名複製至 XDOS/INBOX/。保留基礎安裝中 XDOS/APPS/ 的應用與清單，以及 XDOS/DRIVERS/ 的選配裝置驅動。
4. 安全退出 microSD，插回 Pi 並啟動。UART 為 921600 baud、8N1。桌面自動使用開機韌體偵測或指定的 HDMI 尺寸。

## 既有 XDOS 透過 Ethernet 更新

在同一區網用 Windows／Linux 執行本目錄 tools/xdos-uart.py。先裝好 Python 的 pyserial，將下例 SERIAL_PORT、HOST_LAN_IP 換成自己的序列埠與電腦區網位址。

~~~text
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp PLAYER.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9WIDGET.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9BENCH.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9BROWSE.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9TERM.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp PXLAYOUT.IMG --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 boot PXLAYOUT.IMG --name PXLAYOUT.IMG
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 reset
~~~

每個 net-scp 成功時 XDOS 都會核對 CRC32。若同名檔已安裝，先在主機掛載 SD 檢查其 SHA-256，確認相同後可跳過傳輸。BOOT 應在五個 XAPP 都放妥後執行。完整命令與失敗處理見[區網部署指南](NETWORK_UPDATE_ZH.md)。

## 操作與回退

用滑鼠點桌面圖示或其名稱。MP4 Player 可選 SD 卡上的影片，或在 UART 輸入 PLAY SD:/XDOS/INBOX/DEMO.MP4。可以用 UART CLOSE 停止播放。

保留已驗證的舊核心與它的配對 XAPP。若原本使用 PXBOOT，可用 UART BOOT 選回 PXBOOT.IMG 再 RESET。若無法進入 UART，關機後在電腦掛載 microSD，還原原 config.txt 或將 kernel 改回 PXBOOT.IMG。
