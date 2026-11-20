# XDOS XAPP6 Pi 3B+ 測試

1. 備份 microSD 上的核心、`config.txt` 和原本的兩個 LVGL Demo。
2. 把 `xdos32-v062-rc3-xapp6-pi3.img` 放到開機分割區，將 `config.txt` 的 `kernel=` 設為該檔名。
3. 將本套件的 `XDOS/APPS/` 合併到卡上的同名目錄，以配對的 `LVGLDEMO.APP/.XAPP` 和 `LVGLBENCH.APP/.XAPP` 換掉舊版。其他六個 XAPP 也在套件中。

## 驗收順序

- 開機到桌面，UART 確認 `XDOS V0.6.2-RC3 XAPP6`。
- 點 **LVGL DEMO**，確認 Widgets 圖片和分頁，再返回桌面。
- 點 **LVGL BENCHMARK**，確認官方場景並跑到報告。重開後測試中途退出。
- 交替重跑兩個程式至少三次，確認桌面及 CPU 顯示在退出後恢復。UART 應顯示 `[XAPP] Arduino LVGL setup completed` 和 `[XAPP] Arduino LVGL session closed`。

XAPP5 的映像為 1,856,788 bytes，XAPP6 為 1,529,324 bytes，減少 327,464 bytes（約 319.8 KiB，17.6%）。

Pi 3B+ 啟動時會將獨立的 1 MiB 區段 `0x00300000–0x003FFFFF` 設成可執行。Loader 只使用其中前 320 KiB，載入時清除該工作區並同步資料與指令快取。ELF 的該區段是 NOBITS，對應載入段的檔案大小為零。開機、頁表和交替重開仍待 Pi 3B+ 實機驗收。
