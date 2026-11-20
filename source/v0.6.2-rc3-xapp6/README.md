# XDOS V0.6.2-RC3 XAPP6

[版本下載、Pi 3B／3B+ 安裝與操作指南](OPERATIONS_ZH.md)

從 Windows 用 Ubuntu 24.04 WSL2 建置 Pi 3 ARM32 核心與配套 XAPP。在本目錄執行 `.\build-wsl.ps1 -InstallDependencies`，後續可執行 `.\build-wsl.ps1`。建置輸出在 `wsl-build-output/`。

建置工具固定 Circle 51.1.1 commit `7a81e1b480fe7d19fc1fc695b89bff0a0848f44b`，以及其 LVGL 9.4 commit `c016f72d4c125098287be5e83c0f1abed4706ee5`。驗收映像使用 GCC ARM 13.2.1。相依原始碼由腳本從上游倉庫取得，版本修補與 Arduino sketch 原始碼隨本資料夾提供。

發佈套件基於 RC3 Pi 3 USB MSC source kit。公開版基礎 ZIP 排除了本機部署文件、Wi-Fi 範例設定和預生成輸出。Pi 3B+ 操作流程見 [驗收清單](TEST_PI3B_ZH.md)。核心映像與兩個官方 LVGL Demo 模組以每次建置產生的指紋配對。
