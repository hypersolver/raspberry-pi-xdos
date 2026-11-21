# XDOS XAPP API 與開發指南

本頁以公開的 [PXLAYOUT](PXLAYOUT_ZH.md) 為準。實驗功能另見[研發紀錄](DEVELOPMENT_ZH.md)。確切 C 宣告以對應 Release 的原始碼附件為準。

## 核心與 UI API

| 介面 | 函式或欄位 | 用法 |
| --- | --- | --- |
| XdosHostApiV1 | abi_version、struct_bytes、context | AAPCS32 C ABI。入口先核對版本和表大小，再使用擴充欄位 |
| XdosHostApiV1 | log(context, message) | 輸出 XAPP 訊息 |
| XdosHostApiV1 | show_lvgl_demo、show_lvgl_benchmark | 請求啟動 LVGL Demo／官方 Benchmark。使用擴充欄位前先檢查表大小 |
| XdosKeypadApi | open_keypad(context, title, keys, input) | 建立 Calculator 類按鍵視窗 |
| XdosEditorApi | open_editor(context, title, action) | 建立文字編輯視窗，透過 EDITOR_LOAD、EDITOR_SAVE 回呼 |
| XdosWorkspaceApi | open_canvas(context, title, stroke) | 建立 RGB565 畫布，回呼取得畫布尺寸與筆畫事件 |
| XdosWorkspaceApi | open_workspace(context, panel, title, hint, buttons, action) | 建立擁有輸入欄、按鈕與狀態列的視窗。按鈕以直線分隔，回呼收到由 0 開始的按鈕索引 |
| XdosWorkspaceApi | register_browser_fetch(context, fetch) | Browser 註冊自己的 HTTP 工作函式。函式使用同版核心介面 |
| XdosArduinoLvglApi | run_sketch(context, setup, loop, stop, measured) | 外部 Arduino LVGL 草稿註冊生命週期回呼。Widgets 與 Benchmark 使用同次建置、綁定核心的 XAPP |

PXLAYOUT 的 workspace 面板支援 Files、HTTP、Terminal、Player。請求包括 LIST、LIST_USB、READ、HTTP、CANCEL_HTTP、TERMINAL、PLAY。UPDATE 面板與舊韌體更新請求的常數保留供相容，PXLAYOUT 更新使用 UART 核心命令。Music 實驗面板另加 MUSIC_PLAY、MUSIC_JACK、MUSIC_STOP，目前限開發測試映像。

## 編譯與載入

1. 以 AArch32／ARMv7-A32、AAPCS32 編譯，入口為 xapp_entry(const XdosHostApiV1 *api)。例子見 [MP4 Player](tools/vc4/player_xapp.c) 和 [Terminal](tools/rc3-uart/terminal_xapp.c)。
2. 入口先驗證 ABI 與 struct_bytes，再將 api 轉為對應擴充表。短小且無核心匯入的 XAPP 可使用原位置無關的連結方式。直接匯入 LVGL／核心符號的模組必須用同次建置產生的符號位址和核心指紋。
3. XAPP 檔頭為 32-byte little-endian XDAP。執行檔與啟動清單放在該版本指定的 SD 路徑。核心會檢查檔頭、執行入口與相容條件。驗收包中的 Q9 系列使用核心指紋。
4. 回呼和其資源必須在 XAPP 可執行期間有效。使用核心傳入的 context，避免在回呼長時間阻塞 UI。
5. 在同一核心上測試桌面開啟、操作、關閉、重新開啟。請同時測不相容 XAPP 或檔案缺失時能否回到桌面。

### MP4 Player 使用方式

從桌面點 MP4 PLAYER，在下拉清單選 SD 卡影片後按 PLAY。UART 可用 PLAY SD:/XDOS/INBOX/DEMO.MP4。省略目錄時從 XDOS/INBOX 查找，支援 H.264 影片軌。以 CLOSE 停止目前播放。PXLAYOUT 的尺寸限制及已知問題見[版本指南](PXLAYOUT_ZH.md)。
