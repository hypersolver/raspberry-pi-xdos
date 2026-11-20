# 在 Raspberry Pi 3B／3B+ 建置與使用 XDOS

XDOS 是 ARM32 裸機系統。準備 Pi 3B 或 3B+、microSD、HDMI 螢幕、USB 鍵鼠與電源。區網更新另需乙太網路與 USB–TTL 串列轉接器。

## 選擇版本

| 版本 | 適用情境 | 驗證狀態 |
| --- | --- | --- |
| [RC3 XAPP6 基準包](packages/XDOS_RC3_XAPP6_PI3_TEST.zip) | 首次安裝和確認硬體 | Pi 3B+ 已開機，官方 Widgets 和 Benchmark 可交替執行 |
| [RC3 XAPP6 AppWindows 測試包](packages/XDOS_RC3_XAPP6_PI3_APPWINDOWS_TEST.zip) | 測試五個 XDOS 風格 LVGL 視窗及 VC4 顯示 | 核心與配對 XAPP 已編譯並校驗，整包尚待 HDMI 實機驗收 |

下載後先用 [套件 SHA-256](packages/SHA256SUMS.txt) 核對 ZIP。解壓縮後，內附的 `SHA256SUMS.txt` 可用於比對包內各檔。

```powershell
Get-FileHash -Algorithm SHA256 .\XDOS_RC3_XAPP6_PI3_TEST.zip
```

## 從原始碼編譯

在 Windows 安裝 Ubuntu 24.04 WSL2。若安裝程式要求重啟 Windows，重啟後再執行後續指令。開啟 Ubuntu 一次以完成初始設定。參考 [Microsoft WSL 安裝手冊](https://learn.microsoft.com/windows/wsl/install)。

```powershell
wsl --install -d Ubuntu-24.04
wsl --list --verbose
git clone https://github.com/hypersolver/raspberry-pi-xdos.git
Set-Location .\raspberry-pi-xdos\source\v0.6.2-rc3-xapp6
.\build-wsl.ps1 -InstallDependencies
```

日後用 `.\build-wsl.ps1` 重建。腳本使用固定的 Circle 與 LVGL 版本，從上游取得依賴，輸出位於 `wsl-build-output/<run-id>/`。把核心 IMG、`.APP`、`.XAPP` 和校驗檔一起保存。尤其 Widgets 與官方 Benchmark 必須使用和核心同次建置的 XAPP。AppWindows 測試包的驗收以包內已配對的位元組為準。

在 Ubuntu 終端機重建含 VC4 與 AnalyticsSafe 捲動快取的版本時，切換至此版本目錄，再執行下列指令：

```bash
XDOS_ANALYTICS_MODE=analyticscache bash tools/build-wsl.sh "$PWD"
```

## microSD 首次安裝

1. 先備份卡上的資料與 `config.txt`。若是空白卡，用 Raspberry Pi Imager 先寫入適用 Pi 3 的 Raspberry Pi OS，取得含開機韌體的 FAT 開機分割區。Imager 寫入時會清除所選卡片。參考 [Raspberry Pi 開機媒體指南](https://www.raspberrypi.com/documentation/computers/getting-started.html)。
2. 拔插讀卡機以掛載開機分割區。XDOS 套件內的 `.img` 是核心檔，直接複製到該分割區根目錄。
3. 將測試包的 `XDOS/APPS/` 複製到卡上的同名路徑。若使用自行編譯的產物，將同次建置的 `.APP` 和 `.XAPP` 一起放進該目錄。
4. 在 `config.txt` 的適用區段設定下列選項，核心名稱依實際檔案替換。只保留一個作用中的 `kernel=`；其餘項目以原卡與所選版本需要的設定為準。使用 VC4 GLES2 測試包時，先以 Raspberry Pi 韌體的傳統 framebuffer 啟動，停用 Raspberry Pi OS 設定裡的 `dtoverlay=vc4-kms-v3d`。

   ```ini
   [all]
   arm_64bit=0
   kernel=xdos32-v062-rc3-xapp6-pi3.img
   enable_uart=1
   disable_splash=1
   ```

5. 安全退出 microSD，插回 Pi。接上 HDMI、鍵盤和滑鼠後上電。先核對 XDOS 開機畫面、桌面顯示與鍵鼠，再測試應用程式。

## 使用桌面

| 程式 | 操作 |
| --- | --- |
| Notepad | 用鍵盤輸入。按 **SAVE** 儲存、**LOAD** 讀回，也可用 Ctrl+S 與 Ctrl+O。更新前保留原卡上的使用者資料。 |
| Calculator | 用畫面按鍵或鍵盤輸入算式。 |
| Paint | 按住滑鼠在畫布繪圖。**CLEAR** 會清空畫布。 |
| Browser | 輸入可用的 HTTP 網址並送出。內建瀏覽器提供文字回應，HTTPS 網站不在支援範圍。 |
| Files | 使用畫面按鈕列出 SD 或 USB 檔案。輸入檔名可讀取所選檔案。 |
| LVGL DEMO / LVGL BENCHMARK | 啟動官方 Widgets 或 Benchmark，完成後按 **BACK TO DESKTOP**。 |

AppWindows 版本的前五個程式可拖曳標題列，按紅色 **X** 關閉。現有 XAPP Loader 共用執行區，一次開啟一個 XAPP。

## UART 與區網更新

先確認所選韌體版本支援相應的 UART 指令。AppWindows 測試包使用 **921600 baud、8N1**，早期 XAPP6 基準包使用 **115200 baud**。連接 3.3 V USB–TTL 訊號：轉接器 RX 接 Pi TX（GPIO14，實體 pin 8），TX 接 Pi RX（GPIO15，實體 pin 10），並接共地。Pi 使用自己的電源，轉接器 VCC 留空。COM 編號請從 Windows 裝置管理員確認。

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install pyserial
$uart = '.\.venv\Scripts\python.exe'
$port = 'COM4' # 範例，換成自己的串列埠
& $uart tools/xdos-uart.py --port $port --baud 921600 ping
& $uart tools/xdos-uart.py --port $port --baud 921600 power
```

支援 `net-scp` 的韌體可透過 UART 接收命令，再由 Pi 從同區網電腦的臨時 HTTP 服務取得檔案。先確認有線網路可達。若防火牆詢問，將 TCP 8765 的存取限制在測試 Pi。範例 LAN 位址需替換成電腦在該區網上的 IPv4。

```powershell
$lan = '192.0.2.10' # 文件範例，換成自己的電腦 LAN IPv4
$img = '.\xdos32-v062-rc3-xapp6-perf7-appwindows-pi3.img'
& $uart tools/xdos-uart.py --port $port --baud 921600 net-scp $img --name APPWIN.IMG --bind $lan
& $uart tools/xdos-uart.py --port $port --baud 921600 net-scp .\XDOS\APPS\LVGLDEMO.XAPP --name AWWIDGET.XAPP --bind $lan
& $uart tools/xdos-uart.py --port $port --baud 921600 net-scp .\XDOS\APPS\LVGLBENCH.XAPP --name AWBENCH.XAPP --bind $lan
& $uart tools/xdos-uart.py --port $port --baud 921600 boot $img --name APPWIN.IMG
& $uart tools/xdos-uart.py --port $port --baud 921600 reset
```

將測試包解壓到執行上述指令的位置，或把範例路徑改成實際位置。區網路徑適用已裝有其餘應用程式的卡。`scp` 可改用 UART 傳單檔。兩種傳輸均將檔案放入 `SD:/XDOS/INBOX/` 並校驗 CRC32，既有檔名會被拒絕。`boot` 也會比對核心 CRC32，然後更新 `config.txt`。`reset` 套用選擇。

若新系統可回應 UART，`boot-clear` 後再 `reset` 可以回到前次設定。若無法開機，關機後取出 microSD，手動把 `kernel=` 改回原映像或還原已確認的 `config.txt` 備份。遇到右上角閃電時先檢查供電，可用支援的 UART 韌體執行 `power` 查看是否曾欠壓。
