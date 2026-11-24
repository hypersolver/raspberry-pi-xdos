# C17ABI for Raspberry Pi 3B+

The build retains and checks 5,622 ABI symbol names before generating the kernel export table. It fails if any required name is missing. The ten paired J7 XAPPs are byte-for-byte identical to their FVVIDEO counterparts.

On a Pi 3B+, network upload passed CRC32. The kernel booted at 1920×1080, returned UART READY, and opened and closed all ten J7 shortcuts. Application-specific functions were outside this test. The board was restored to FVWEP1, with UART, wired DHCP and keyboard and mouse detection confirmed.

Verify the downloaded IMG and XAPPs against `SHA256SUMS.txt`. Keep a known-good boot image on the microSD. On an existing XDOS card, upload `C17ABI.IMG` to the inbox over the network, select it with `BOOT`, then `RESET`. On a prepared XDOS card, copy the IMG to the boot partition and set `kernel=C17ABI.IMG` in `config.txt`. Keep the existing J7 files or place the accompanying files in `XDOS/INBOX`. To roll back, select the previous IMG and reboot.

## 繁體中文

建置會保留並核對 5,622 個 ABI 符號，再產生核心匯出表。少了任何必要符號即停止建置。十個配對 J7 XAPP 與 FVVIDEO 的版本逐位元相同。

Pi 3B+ 實測通過網路上傳 CRC32、1920×1080 開機及 UART READY。十個 J7 捷徑均可開啟與關閉。各 App 的個別功能未逐項重測。實測後恢復 FVWEP1，確認 UART、有線 DHCP 和鍵鼠偵測正常。

下載後用 `SHA256SUMS.txt` 核對映像及 XAPP。保留原開機映像供回退。現有 XDOS 可透過網路把 `C17ABI.IMG` 傳入 INBOX，再用 `BOOT` 選擇並執行 `RESET`。也可把 IMG 複製到已準備好的 XDOS microSD 開機分割區，將 `config.txt` 的 `kernel=` 設為 `C17ABI.IMG`。J7 檔案可沿用，或將配對檔放到 `XDOS/INBOX`。回退時選擇舊映像並重開。
