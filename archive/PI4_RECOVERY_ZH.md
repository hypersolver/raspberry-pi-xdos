# Pi 4 早期 XDOS 版本復原

2026-11-21 從舊 Pi 4 編譯目錄取回兩份核心映像與其所在的 XDOS 原始碼。新版 ZIP 由取回檔案重新封裝。保留的核心映像位元組經 SHA-256 核對，與 Pi 4 上的檔案一致。

| 版本 | Circle 範例目錄 | `kernel8-32.img` SHA-256 |
| --- | --- | --- |
| V0.5 | `sample/94-xdos-arm32-boot` | `344059c77b071e14da782e5cc446cdb7a3e2c07e13d92fcdd6ff47e407077765` |
| V0.5.2 | `sample/95-xdos-arm32-dualnet` | `bf5ff4caea82ebaf5cb945682b94bfe2181b19cfe2f7efb04f8bb3d231a95500` |

以原始碼和映像內的開機版本字串辨識。V0.5 的 About 與桌面字串仍有 V0.4 標記，但開機字串為 V0.5。V0.5.2 的開機字串為 `V0.5.2: AUTO Ethernet/WiFi + HTTP Timeout`。

測試前先核對 [ZIP SHA-256](SHA256SUMS.txt)。如要重現歷史開機，先備份 microSD 的現有核心與 `config.txt`，再將映像放到含相容 Pi 開機韌體的 FAT 分割區，讓 `kernel=` 指向該檔並使用 ARM32 開機。這次復原未重新開機測試。

原始資料夾的 Makefile 保留相對於 Circle 的編譯位置。兩版需要相容的 Circle ARM32／Pi 3 靜態庫，使用 Wi-Fi 時還需 WLAN 與相關韌體。這次沒有取得原發行 ZIP、完整依賴快照或可重現的原始編譯設定。
