# XD OS V0.6.2-RC3 HF7 開發狀態

目標平台是 Raspberry Pi 3B ARM32，使用 Pi 4 作交叉編譯主機。HF7 的目標是以外部 Native XAPP 執行 LVGL v9.4 官方 Widgets Demo 與 Benchmark。

| 階段 | 已取得的證據 |
| --- | --- |
| 原版來源建置 | 20 份官方來源已編成 ARM32 可重定位物件。合併物件為 652,884 bytes，包含 7 個原版圖片資源符號。 |
| LVGL Runtime 稽核 | 在 Circle LVGL 靜態庫找到 219/219 個函式名稱。仍需整合 `lv_font_montserrat_26`，並處理 `halt` 的應用程式錯誤策略。 |
| ARM32 Import Bridge | 219 個呼叫跳板與 876-byte 函式表已生成並通過組譯。官方 Demo 的完整連結仍待完成。 |
| 記憶體與 MMU 稽核 | 確認原執行區為 32 KiB。大型執行區的所有權、頁面權限及載入後驗證仍待確認。 |
| 大型 XAPP 測試 | 已有診斷與 smoke test 套件。本次盤點尚無可對應雜湊的 Pi 3B 驗收紀錄。 |

LVGL 稽核使用 commit `c016f72d4c125098287be5e83c0f1abed4706ee5`。符號名稱吻合只涵蓋靜態檢查。完整發布仍需驗證 ABI、重定位、資料匯入、GUI 回呼生命週期，以及 Pi 3B 執行結果。

V0.6.2-RC3 USB MSC 為目前候選建置基線。GitHub Release 的核心映像需與實際 Pi 3B 驗收結果及 SHA-256 對應。HF7 的完整官方 Widgets Demo 與 Benchmark 尚未列入已驗收功能。
