# Raspberry Pi XDOS 操作手冊

範例中的 `/path/to/...`、`BUILD_USER`、`BUILD_HOST` 和 `<MicroSD boot volume>` 都是佔位符。執行前請換成自己的建置目錄、連線設定或已確認的開機分割區位置。

## 1. 準備版本檔案

以 V0.1、V0.2 為單位整理各自的原始碼、組譯檔、建置腳本、開機設定與編譯產物。若原始碼以 ZIP 保存，分別解壓到不同目錄，避免建置時混入另一版檔案。先記錄 Raspberry Pi 型號、工具鏈和 Circle 版本。

V0.1 在 Pi 4 Ubuntu ARM64 安裝 clang、lld、llvm、make 與 file 後，進入 source/v0.1 執行 `bash build-v01-on-pi5.sh`。將產出的 `xdos32.img` 放到 Pi 3B+ 的開機分割區，使用套件所附的 `config.txt`（`kernel=xdos32.img`）。詳見 [Pi 4 建置說明](source/v0.1/README_PI5_UBUNTU.md)。

## 2. 在 Pi 4 編譯 V0.2

將 [V0.2 原始碼](source/v0.2/README.md)所在的 source/v0.2 目錄傳到 Pi 4。使用 SSH 登入 Pi 4，切換至含有 `build-on-rpi4.sh` 的原始碼根目錄，再執行：

```bash
bash build-on-rpi4.sh /path/to/circle
```

建置成功後列出產物及校驗值：

```bash
ls -lh output/*.img
sha256sum output/*.img
```

記下本次使用的原始碼版本、建置日期、實際輸出檔名和 SHA-256。原版 V0.2 的映像為 `xdos-arm32-asm-v02.img`。DEBUG 版在對話中使用 `xdos-arm32-asm-v02-debug.img`。

## 3. 寫入 MicroSD 開機分割區

1. 將 MicroSD 接到電腦，備份現有 `config.txt`。
2. 將本次編譯的 `.img` 複製到已有 Raspberry Pi 開機韌體的分割區根目錄。這裡的 `.img` 是由韌體載入的核心檔案。
3. 開啟該分割區的 `config.txt`，依測試使用的版本設定 `kernel=`。V0.2 的開機設定見 [config-asm-v02.txt](source/v0.2/config-asm-v02.txt)。
4. 比對 `kernel=` 與磁碟上的檔名，包含副檔名。V0.2 曾因把 `xdos` 寫成 `xds` 而停在 Splash。
5. 安全退出 MicroSD，插回 Pi 3B，接上 HDMI、USB 鍵盤和滑鼠後開機。

## 4. 驗證與記錄

每個版本分別記錄能否進入桌面、鍵盤能否輸入、滑鼠能否移動和點擊，以及拖曳視窗時是否閃爍。若停在 Splash，先核對 `kernel=` 與 MicroSD 根目錄的檔名。若使用 DEBUG 映像，記下畫面最後一條診斷訊息。

V0.2 已確認能開機，鍵盤與滑鼠正常，拖曳視窗時仍會閃爍。V0.1 套件記錄 Pi 3B+ 已開機並顯示靜態 HDMI 桌面。建置主機為 Pi 4 Ubuntu ARM64。

## 5. 發布到威凱 GitHub

在指定的公開 GitHub repository 執行以下步驟。

1. 在目標 repository 提交 V0.1 對應的原始碼與建置檔，將 V0.1 標籤指向該版提交。
2. 提交 V0.2 對應的原始碼與建置檔，將 V0.2 標籤指向該版提交。
3. 推送提交與標籤。在 GitHub 的 Releases 頁面，為兩個標籤各建立一筆 Release。
4. 每筆 Release 附上該版本實際編譯的 `.img` 與 SHA-256，並填入編譯環境和實機測試結果。若同時發布原版和 DEBUG 版，分別標明檔名與測試結果。
5. 從 GitHub 重新下載各版本的映像並核對 SHA-256。在 Windows PowerShell 可執行 `Get-FileHash -Algorithm SHA256 -LiteralPath '實際映像檔路徑'`。


V0.3 使用者實測：從開機畫面出現到桌面完整開啟約需 3 秒。
