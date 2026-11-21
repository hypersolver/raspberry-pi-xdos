# 第 2 篇 · Arduino 草稿開發、部署與執行

先依[第 1 篇](xdos-quick-start.zh-TW.md)讓 Pi 3B+ 進入 XDOS 桌面。這一篇用 PXLAYOUT 的 Arduino Widgets 和 Benchmark 示範 XAPP。之後只更新配對的應用檔時，可以留下既有核心和開機設定。

## 開發 Arduino 草稿

可用 Arduino IDE 開啟下列 `.ino` 草稿：

| 範例 | 內容 |
| --- | --- |
| [XappBasic](../source/v0.6.2-rc3-xapp6/arduino-demos/XappBasic/XappBasic.ino) | 基本 `setup()`／`loop()`。開啟後可用除錯器查看 `iterations` 計數。畫面提供返回桌面的按鈕。 |
| [XappLvgl](../source/v0.6.2-rc3-xapp6/arduino-demos/XappLvgl/XappLvgl.ino) | 獨立 LVGL 範例。每次點擊畫面中央的按鈕，計數應加一。 |
| [XappMedia](../source/v0.6.2-rc3-xapp6/arduino-demos/XappMedia/XappMedia.ino) | 顯示照片、播放 H.264 MP4、透過 HDMI 播放與停止 WAV。需要含影音功能的 XDOS 測試核心。 |
| [LVGL Widgets](../source/v0.6.2-rc3-xapp6/arduino-demos/LVGLWidgetsDemo/LVGLWidgetsDemo.ino)／[官方 Benchmark](../source/v0.6.2-rc3-xapp6/arduino-demos/LVGLBenchmarkDemo/LVGLBenchmarkDemo.ino) | 完整元件與效能測試。 |

- `setup()` 建立畫面和物件。XDOS 已初始化顯示與 LVGL。
- `loop()` 每次做少量工作後返回，讓 XDOS 可繼續處理輸入和畫面。`RPIDUINO_LVGL_HOST_TIMER` 啟用時由 XDOS 驅動 LVGL 計時。
- 若建立了在頁面關閉後仍會執行的計時器或回呼，應像 Benchmark 的入口一樣提供停止函式，清掉資源再回到桌面。

現有草稿可從原始碼建成 XAPP。若手上只有 Arduino IDE 匯出的裸機 IMG，請回到 `.ino` 原始碼，以 XAPP 入口及 XDOS Host API 重新建置。直接存取硬體的函式庫須確認有對應 XDOS 服務。 [XAPP ABI 與開發 API](../source/v0.6.2-rc3-xapp6/XAPP_API_ZH.md) · [草稿入口與停止回呼](../source/v0.6.2-rc3-xapp6/tools/rc3-uart/sketch_xapp.cpp)

## 編譯 XAPP

1. 依 [XDOS WSL 建置說明](../source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md#從原始碼編譯) 安裝 ARM32 工具鏈。在 `source/v0.6.2-rc3-xapp6` 修改上述草稿。
2. 在 PowerShell 執行 `.\build-wsl.ps1`。建置流程編譯 Arduino 草稿並產生 `LVGLDEMO.XAPP`、`LVGLBENCH.XAPP` 和對應的核心。若需重現 PXLAYOUT 的配對命名和版本，依 [PXLAYOUT 原始碼建置說明](../source/v0.6.2-rc3-xapp6/PXLAYOUT_SOURCE.md) 使用同版產物。
3. 保留同次建置的核心與 XAPP。直接匯入 LVGL 符號的 XAPP 綁定核心指紋，變更工具鏈或重新連結核心後要重新核對配對。初次試跑可直接用 Release 的 `Q9WIDGET.XAPP` 和 `Q9BENCH.XAPP`。

要測試兩個短範例，在 WSL 的 `source/v0.6.2-rc3-xapp6` 目錄執行下列其中一條。一次選一個草稿。這兩個測試建置會使用 **LVGL Demo** 捷徑的 `LVGLDEMO.XAPP` 檔名，依建置附帶的清單與說明部署配對的核心和 XAPP。

```bash
XDOS_WIDGETS_SKETCH=XappBasic bash tools/build-wsl.sh "$PWD"
XDOS_WIDGETS_SKETCH=XappLvgl bash tools/build-wsl.sh "$PWD"
```

## 部署與執行

| 情境 | 操作 |
| --- | --- |
| microSD 部署 | 先關閉 Pi。將配對的 PXLAYOUT XAPP 複製到卡上的 `XDOS/INBOX/`，保留 Release 原檔名。若自行建置，依該建置的 `.APP` 啟動清單及對應版本說明放置檔案。 |
| Ethernet 部署 | 在 XDOS 已啟動時，用 `xdos-uart.py net-scp` 傳送配對 XAPP。UART 觸發下載，檔案走 Ethernet，XDOS 驗證 CRC32。現有傳輸會拒絕覆寫同名檔。第一次可傳到空的目標位置。更新同名檔時可關機，在電腦備份舊檔，再由 microSD 複製新版。 |
| 執行 XAPP | 先關閉目前的 XAPP，再點對應桌面捷徑。PXLAYOUT 的 `Q9WIDGET.XAPP` 對應 **LVGL Demo**，`Q9BENCH.XAPP` 對應 **LVGL Benchmark**。使用 **BACK TO DESKTOP** 結束。 |

以尚無同名檔的 PXLAYOUT 測試卡為例，從專案版本目錄執行。將序列埠和 Windows 端區網位址換成實際值。

```powershell
python -m pip install pyserial
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 net-scp Q9WIDGET.XAPP --bind HOST_LAN_IP
python tools/xdos-uart.py --port SERIAL_PORT --baud 921600 open WIDGETS
```

應用部署只需傳 XAPP。當核心相容時不用執行 `BOOT` 或 `RESET`。 [PXLAYOUT 網路部署](../source/v0.6.2-rc3-xapp6/PXLAYOUT_ZH.md#既有-xdos-透過-ethernet-更新)

## Arduino IDE「上傳」目前的範圍

RPiDuino 0.1.2 在 IDE 的「上傳」仍用於獨立 IMG。XDOS XAPP 目前使用本篇的 WSL 建置與既有 XDOS 部署工具。統一成 IDE 內的 **XDOS XAPP → 上傳**，仍待板卡建置配方與 XDOS 部署流程整合。這篇的可執行範例適用已配對的 Pi 3B+ XDOS 核心。

## 動態桌面捷徑

使用新版 `build_player.sh` 建置的核心會在 FAT32 掛載後掃描 `SD:/XDOS/APPS/`，依 FAT 目錄讀取順序收錄前 **10** 個符合格式且找得到執行檔的 `.APP` 檔案。壞清單、子目錄和找不到執行檔的項目會跳過。超過上限的項目保留在卡上。變更清單後重新開機即可重新掃描。

圖示使用 `Name=` 中第一個英文字母，統一顯示大寫。A–Z 各對應一種固定顏色，常見首字母採用較容易彼此分辨的色相。名稱沒有英文字母時顯示灰底 `?`。

桌面名稱置中在圖示下方，最多顯示 16 個字元。名稱超過 16 個字元時會隱藏整行，仍可點圖示開啟。

捷徑固定排成上下兩排、每排五格，依掃描順序由左到右填入。圖示與名稱共用每格的中心線。

Arduino XAPP 的清單範例：

```ini
[Application]
Name=Arduino Demo
Exec=SD:/XDOS/APPS/ARDDEMO.XAPP
ABI=1
Arch=armv7-a32
Type=Arduino
```

把上述內容存為 `ARDDEMO.APP`，並將配對的 `ARDDEMO.XAPP` 放入 `XDOS/APPS/`。點擊圖示或名稱即可交給載入器。載入器在啟動時會核對 XAPP 的 ABI 和核心指紋。既有 PXLAYOUT Release 使用固定捷徑。

## 影音範例 XappMedia

這個範例使用 **已建置完成、同時包含 MP4 Player 和 Music 的 Pi 3B+ 測試核心**。公開 PXLAYOUT 的音樂服務不可用。準備一張 JPG 或 PNG，建置工具會把它轉成最多 320×180 的 RGB565 圖片並嵌入 XAPP。

在 WSL 使用已安裝 Pillow 的 Python 執行以下命令。第二個參數是 Circle `sample/` 下已完成影音建置、含 `music_host.inc` 的 XDOS 目錄。腳本會複製該目錄進行測試建置。

```bash
python3 tools/vc4/build_arduino_media.py /path/to/circle /path/to/circle/sample/audio-build photo.jpg ./media-build
```

1. 另用測試 microSD，把 `MXMEDIA.IMG` 放到開機分割區，將 `kernel=` 指向這個 IMG，並保留原 `config.txt` 以便回退。
2. 把 `MXWIDGET.XAPP`、H.264 MP4（命名為 `DEMO.MP4`）和 44.1 kHz PCM WAV（命名為 `DEMO.WAV`）複製到 `XDOS/INBOX/`。影片可先用低解析度測試。
3. 開機後點 **LVGL Demo**。照片應顯示在按鈕上方。按 **Play MP4** 測試影片，按 **Play WAV** 測試 HDMI 音樂，按 **Stop** 停止音樂。回桌面時也會停止音樂。

這次建置只重新配對影音示範的 XAPP。其他會檢查核心指紋的 XAPP 需重新配對後才能執行。照片轉換、核心及 XAPP 的建置驗證和 Pi 上的畫面、聲音驗證是不同步驟。
