# XD OS ARM32 V0.3：RPi 3B + Circle Ethernet + Browser + 降低拖曳閃爍

## 架構

- 編譯主機：RPi 4，Ubuntu 24.04 ARM64
- 目標硬體：RPi 3B，Cortex-A53，ARM32 Bare-metal
- 使用既有 Circle：`~/xdos-build/circle`，`RASPPI=3`、`AARCH=32`
- 主要 GUI 功能仍透過 `xdos_gui.S` ARM32 組合語言處理
- Circle 負責 USB/鍵鼠/HDMI/LAN9514/TCP-IP 等底層

## V0.3 新增

1. RAM Back Buffer：在記憶體完成繪圖，再複製到顯示緩衝區。
2. 拖曳 Dirty Rectangle：只複製視窗移動前後影響區域（仍會在 RAM 內重新生成全畫面）。
3. 單一游標：不反覆隱藏／顯示 Circle 內建游標，滑鼠移動只更新 14×20 鄰近像素。
4. LAN9514 USB Ethernet、DHCP、DNS、TCP/IP。
5. Browser：輸入 HTTP URL、下載 HTML、抽取可見文字。網路請求在 Circle 排程工作中執行。
6. Terminal `net` 查看 IP、`browse http://...` 開啟 Browser。

**限制：不支援 HTTPS、JavaScript、CSS、圖片、連結點擊、301/302 自動轉址、自訂連接埠。Browser 接收上限 48 KiB、顯示文字上限 12 KiB。**

## 先由 Windows 傳送 ZIP

Windows PowerShell 執行（下載 ZIP 到 Downloads 後）：

```powershell
scp ".\XDOS_ARM32_ASM_V0.3_NET_Browser_RPi3B.zip" BUILD_USER@BUILD_HOST:/path/to/xdos-build/
```

## RPi 4 編譯

```bash
cd ~/xdos-build
unzip -o XDOS_ARM32_ASM_V0.3_NET_Browser_RPi3B.zip
cd XDOS_ARM32_ASM_V0.3_NET_Browser_RPi3B
bash build-on-rpi4.sh
ls -lh output/xdos32-v03.img
```

若顯示 Circle 缺少 lib/net 或 sched 函式庫，請先執行：

```bash
cd ~/xdos-build/circle
./makeall
```

## 傳回 Windows

Windows PowerShell：

```powershell
scp BUILD_USER@BUILD_HOST:/path/to/xdos-build/XDOS_ARM32_ASM_V0.3_NET_Browser_RPi3B/output/xdos32-v03.img ".\"
```

將 `xdos32-v03.img` 放入 RPi 3B MicroSD FAT32 根目錄。保留原先已確認可開機的韌體與其它設定，只將 `config.txt` 的 kernel 指向：

```ini
[all]
arm_64bit=0
kernel=xdos32-v03.img
```

## 驗收流程

1. 啟動 RPi 3B（HDMI + USB 鍵鼠 + 網路線到 DHCP Router）。
2. 測試移動與拖曳視窗；應比 V0.2 減少閃爍，但沒有 VSync 硬體翻頁時仍可能撕裂。
3. 待網路取得 IP；Terminal 輸入 `net` 可看到 IPv4。
4. Browser 視窗用滑鼠點網址欄清空，輸入 `http://<RPi4的LAN_IP>/`，按 Enter。
5. 在 RPi 4 的另一個 SSH 終端設定測試網頁服務：

```bash
mkdir -p ~/xdos-build/webtest
printf '<html><body><h1>XD OS NETWORK OK</h1><p>Pi 3B HTTP test.</p></body></html>\n' > ~/xdos-build/webtest/index.html
sudo python3 -m http.server 80 --bind 0.0.0.0 -d ~/xdos-build/webtest
```

**第一次測試直接用 RPi4 的 LAN IPv4，不要用 `<mDNS-hostname>`**。Circle DNS 不是必然支援 mDNS。若執行環境有防火牆須開放 TCP/80。

## 除錯

- 啟動檢查點：`[V03-01]`～`[V03-07]`（最後一項只走 UART）。
- HDMI 無畫面：核對 `config.txt` 的 `kernel=xdos32-v03.img` 與 SD 實際檔名。
- 鍵鼠正常、NET WAIT：確認 RPi3B 的 Ethernet 線／Link、路由器 DHCP、RPi3B 的 MAC/IP。
- Browser: `HTTP timeout`, `DNS resolution failed`, `HTTP connection reset` 或 `HTTP 200 OK`。
- 保留 `xdos-arm32-asm-v02-debug.img`，出問題即可恢復原 `kernel=` 行回退。

## 發佈及授權

原版 XD OS 使用 MIT，Circle 使用 GPLv3。這個移植版本連結 Circle 時須遵守其 GPLv3 發佈義務，請保留上游授權通知。

## 驗證聲明

已完成原始碼組譯（ARM32 ASM）、桌面邏輯的 native smoke test、語法檢查。尚未在使用者 RPi4 的 Circle 原始碼樹完成整體連結，也未在 RPi3B 實機測試網路及 Browser。請回傳 RPi4 編譯輸出與 RPi3B 執行結果以便定位問題。


Replace BUILD_USER, BUILD_HOST and /path/to/xdos-build with your build host settings before running the examples.
