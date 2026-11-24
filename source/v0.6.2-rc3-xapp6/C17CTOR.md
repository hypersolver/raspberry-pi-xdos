# C17CTOR for Raspberry Pi 3B+

The V2 packer preserves the C++ initializer table and runs it before calling the XAPP entry point. Rebuild a sketch with the updated packer to enable this behavior. Existing J7 modules remain usable.

Pi 3B+ validation passed network CRC32, boot and UART READY. The test XAPP reported `[CTOR] PASS state=42`, confirming that its static constructor ran before entry. All ten existing J7 shortcuts opened and closed without reuploading their files. The newly packed J7 set passed SHA-256 verification. Individual application features were outside this test. The Pi was restored to FVWEP1 with UART and wired DHCP verified.

Verify the IMG and ten J7 files against `SHA256SUMS.txt`. On an existing XDOS installation, upload the IMG to INBOX, select it with `BOOT`, then run `RESET`. Alternatively, copy the IMG to the prepared microSD boot partition and set `kernel=C17CTOR.IMG` in `config.txt`. Existing J7 modules can stay in place. If installing the newly packed J7 set, back up the old copies before replacing them. To roll back, select the previous boot image and restart.

## 繁體中文

V2 打包器會保留 C++ 初始化表，在呼叫 XAPP 入口前執行靜態建構子。使用新版打包器重建草稿即可啟用，既有 J7 模組仍可使用。

Pi 3B+ 實測通過網路傳輸 CRC32、開機與 UART READY。測試 XAPP 回報 `[CTOR] PASS state=42`，確認靜態建構子在入口前執行。十個既有 J7 捷徑皆可開啟與關閉，無需重傳；本次重新打包的 J7 則已核對 SHA-256。各 App 的個別功能未逐項重測。測完已切回 FVWEP1，確認 UART 與有線 DHCP 正常。

下載後用 `SHA256SUMS.txt` 核對核心與十個 J7。現有 XDOS 可將 IMG 傳到 INBOX，用 `BOOT` 選擇後執行 `RESET`。也可將 IMG 複製到已準備好的 microSD 開機分割區，在 `config.txt` 設定 `kernel=C17CTOR.IMG`。既有 J7 可沿用。若要安裝重新打包的 J7，請先備份同名舊檔。回退時選回原本的開機映像並重開。
