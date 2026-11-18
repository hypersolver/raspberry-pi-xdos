# XD OS ARM32 V0.5.3-RC — 空白桌面啟動 + 網路延後偵測

這是 V0.5.2 的小幅修正版，針對使用者已確認可開機的單一 Ethernet+Wi-Fi 映像。

**兩項變更**

1. `desktop.cpp`：Terminal、Notepad 及其他應用程式啟動時均為關閉；桌面捷徑和工作列按鈕仍可手動開啟。原有 Terminal 開機記錄保留在緩衝區。
2. `kernel.cpp`：在 `Initialize()` 時不做 Ethernet PHY 探測、Wi-Fi 韌體讀取、WPA2、DHCP 與 HTTP Worker 建立；第一次桌面 Framebuffer 畫好後，`Run()` 建立 `CNetworkInitTask`，等 350 ms 後才開始偵測。開機 Splash 會標註 `NETWORK deferred`，不等待 DHCP。

**限制**：Circle 的協作式排程並非搶先式多工。若某些 USB/SDIO/網路驅動初始化函式內部長時間忙等，進入桌面後仍可能短暫停止 UI 回應。此版確保網路初始化不在開機 Splash 路徑，但不保證 Wi-Fi 初始化時完全無停頓。

**使用既有編譯主機**：RPi 4、Ubuntu 24.04.5 ARM64；目標是 RPi 3B／3B+ AArch32，不是 Pi4 的執行映像。ASM GUI 的 `xdos_gui.S` 保留，Circle USB/WLAN/HTTP 驅動仍是 C/C++。

## 編譯與部署

Windows PowerShell 把下載的 ZIP 送往 RPi4：

```powershell
scp ".\XDOS_ARM32_ASM_V0.5_3_DESKTOP_FIRST_RPi3B.zip" BUILD_USER@BUILD_HOST:/path/to/xdos-build/
```

在 RPi4 SSH：

```bash
cd ~/xdos-build
unzip -o XDOS_ARM32_ASM_V0.5_3_DESKTOP_FIRST_RPi3B.zip
cd XDOS_ARM32_ASM_V0.5_3_DESKTOP_FIRST_RPi3B
bash build-on-rpi4.sh
ls -lh output/xdos32-eth-wifi.img
```

在 Windows PowerShell 取回：

```powershell
scp BUILD_USER@BUILD_HOST:/path/to/xdos-build/XDOS_ARM32_ASM_V0.5_3_DESKTOP_FIRST_RPi3B/output/xdos32-eth-wifi.img ".\xdos32-v053.img"
```

複製 `xdos32-v053.img` 到 MicroSD FAT32 根目錄，僅修改原本 `config.txt`：

```ini
kernel=xdos32-v053.img
```

原本 `firmware/`、`wpa_supplicant.conf` 及 HDMi 設定均維持不變，勿覆蓋成功開機的舊映像檔。

## 驗收

1. 開機完成只見桌面圖示、工作列及網路狀態框；沒有 Terminal/Notepad 視窗。
2. 開機 Splash 顯示網路延後偵測；不在 Splash 等候 ETH PHY、WPA2 或 DHCP。
3. 進桌面後網路狀態由 `NET: STARTS IN DESKTOP` 變成 `ETH: DETECTING...`，再選 ETH 或 Wi-Fi，最後顯示 DHCP IP。
4. 手動點 Terminal 或 Notepad 可正常使用；Terminal 仍保留 Boot Log。
5. Browser 的 HTTP Timeout / Cancel 功能不變。

## 授權與安全

沿用原 ZIP 的上游授權。`wpa_supplicant.conf` 含密碼，請勿公開散布。

---

## V0.5.2 原始說明（供參考）

# XD OS ARM32 V0.5.2-RC — 單一 Ethernet + Wi-Fi 自動選擇映像

## 重要說明

這份 ZIP 是供 **RPi4 Ubuntu 24.04 ARM64 交叉編譯** 的完整原始碼，不是已經完成實機驗證的 `.img`。

最終在 RPi 4 產生唯一的 `output/xdos32-eth-wifi.img`，供 **RPi 3B 或 RPi 3B+ ARM32 Bare-metal** 使用。

**網路策略（開機時選擇一次）：**

- 檢測到 LAN9514/USB Ethernet 的 PHY link up：選擇 Ethernet + DHCP；為避免 SDIO/Wi-Fi 啟動造成額外風險，Wi-Fi 略過。
- 沒有 Ethernet link：從 microSD FAT32 掛載官方 Wi-Fi 韌體，啟動 SDIO WLAN、WPA2-PSK、DHCP。
- Wi-Fi 韌體或 FAT32 初始化失敗：回退到 Ethernet TCP/IP，等待接上網路線。
- Circle 只啟動一個 TCP/IP 網路子系統；**目前不支援兩張卡同時各自有 IP，也不支援運作中拔插網路線自動切換**。換介面請重新開機。
- 即使沒有網路，桌面、滑鼠與鍵盤仍啟動。Browser 維持 HTTP-only 和載入期限，不支援 HTTPS。

## RPi 4 Ubuntu 編譯

先在 Windows 下載本 ZIP，PowerShell：

```powershell
scp ".\XDOS_ARM32_ASM_V0.5_2_ETH_WIFI_AUTO_RPi3B.zip" BUILD_USER@BUILD_HOST:/path/to/xdos-build/
```

在 RPi 4 SSH（第一次建議確認已安裝 `wget` 與 `zip`：`sudo apt install -y wget zip`）：

```bash
cd ~/xdos-build
unzip -o XDOS_ARM32_ASM_V0.5_2_ETH_WIFI_AUTO_RPi3B.zip
cd XDOS_ARM32_ASM_V0.5_2_ETH_WIFI_AUTO_RPi3B
bash build-on-rpi4.sh
ls -lh output/xdos32-eth-wifi.img
```

Circle 應已存在於 `~/xdos-build/circle`，`Config.mk` 必須設定 `RASPPI = 3`、`AARCH = 32`、`PREFIX = arm-none-eabi-`。編譯腳本新增 `circle/sample/95-xdos-arm32-dualnet`，不會覆寫已能開機的 `94-xdos-arm32-boot`。注意它仍會對 Circle `lib/net` 的 TCP Connect 超時進行帶備份的共用修補。

## 準備 Wi-Fi 韌體

在 RPi 4 同一專案目錄執行：

```bash
bash tools/prepare-wifi-firmware-on-rpi4.sh
ls -lh output/wifi-firmware.zip
```

Windows PowerShell 把產物下載到 Downloads：

```powershell
scp BUILD_USER@BUILD_HOST:/path/to/xdos-build/XDOS_ARM32_ASM_V0.5_2_ETH_WIFI_AUTO_RPi3B/output/xdos32-eth-wifi.img ".\"
scp BUILD_USER@BUILD_HOST:/path/to/xdos-build/XDOS_ARM32_ASM_V0.5_2_ETH_WIFI_AUTO_RPi3B/output/wifi-firmware.zip ".\"
```

把 `xdos32-eth-wifi.img` 放入 microSD FAT32 **根目錄**；把 `wifi-firmware.zip` 解壓至同一個 FAT32 根目錄，必須保留 `firmware/` 資料夾層級。

在裝置端依 Circle WLAN 文件建立 MicroSD 根目錄的 wpa_supplicant.conf，設定當地法規區域與連線資訊。

## MicroSD 設定

```ini
[all]
arm_64bit=0
kernel=xdos32-eth-wifi.img
enable_uart=1
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=16
disable_splash=1
gpu_mem=64
```

**保留**原本能啟動的 V0.3/V0.5 映像檔，以便回復。首次建議先插 Ethernet 網路線檢查，再拔線重開機檢查 Wi-Fi；不要在通電時直接拔 SD 卡。

## 診斷訊息

開機 HDMI 與 UART 診斷會標示 `ETH PROBE`、`ETHERNET`、`WIFI SD`、`WIFI FW`、`WIFI/DHCP`。桌面 Network 狀態顯示 `ETH 192...` 或 `WIFI 192...`（均為實際 IP），Terminal 的 `net` 命令可查 IP。Browser 的 5 秒接收閒置、20 秒總載入軟期限及 STOP/Esc 取消保留。

此版本仍未在完整 Circle 原始碼上連結或實機驗證，若 RPi4 報錯，請提供第一筆 `error:` 及最後 40 行輸出。

## 授權

原版 XD OS 採 MIT License；Circle、WLAN/WPA 元件包含 GPLv3 與其他上游授權。再次散布映像及整合原始碼時應依相應授權提供 notices/source，並注意 Wi-Fi 韌體的授權條款。不要把含真實 Wi-Fi 密碼的設定檔公開。


建置環境為 Raspberry Pi 4 Ubuntu ARM64。BUILD_USER、BUILD_HOST 與 /path/to/xdos-build 為佔位符。Wi-Fi 連線設定需在裝置端建立。
