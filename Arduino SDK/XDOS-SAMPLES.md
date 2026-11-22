# RPiDuino 0.1.4 的 XDOS 範例

在 Pi 3B+ 開機進入與套件配對的 FILEQ128。Windows 需要 Arduino IDE、WSL、
arm-none-eabi 工具鏈與 Python pyserial。

1. 安裝 RPiDuino 0.1.4 板卡套件並選擇 RPiDuino 3/3B+。
2. 選擇工具選單的 Build target → XDOS XAPP。
3. 從 RPiDuino Basics 的 XDOS 範例選單開啟草稿，按驗證或編譯。
4. 選擇連接 Pi debug UART 的 COM 埠，按上傳。上傳工具會驗證 CRC32，
   並顯示 SD:/XDOS/INBOX 下的檔名。從 XDOS Files 點選該檔即可啟動。

| 草稿 | XDOS 功能 |
| --- | --- |
| XappBasic | setup / loop 最小範例 |
| XappBlink | 板載 ACT LED 和 LVGL 狀態 |
| XappLvgl | LVGL 點擊計數 |
| XappImageView | 從 SD 或 USB 選擇 BMP 圖片 |

套件附有 FILEQ128 的 XAPP 匯入符號與標頭。其他核心建置須用
RPIDUINO_XDOS_HOST 指向該版從配對 ELF 和 IMG 匯出的 xapp-host.json。
若 LVGL 或 FatFs 設定不同，請同步提供該版 SDK 標頭
（RPIDUINO_XDOS_HEADERS）。XDOS 使用核心指紋驗證配對。

要建置傳統開機 IMG，選擇 Standalone IMG。Pi 2、4、5 目前沿用其
既有裸機建置配方。
