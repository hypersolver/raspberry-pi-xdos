# XDOS 核心區段盤點

## PXLAYOUT · 目前公開實測版

IMG 為 1,491,628 bytes，SHA-256 為 d4c1ce474e517b4ae1fbd43609021718069c3800db2c09980abbfe46bea95097。以下數值來自此 IMG 同次建置的 ARM32 ELF 與連結表。音訊等後續實驗映像不納入這組尺寸。

| 區段 | 大小 | 位於 IMG | 判斷 |
| --- | ---: | :---: | --- |
| .init | 116 B | 是 | 開機入口與組合語言啟動流程，保留 |
| .text | 1,240,776 B | 是 | 占 IMG 約 83.2%。保留必要程式，優先移除未使用的功能與函式 |
| .rodata | 249,288 B | 是 | 占 IMG 約 16.7%。字型、鍵位、訊息、表格等可逐項縮減 |
| .init_array | 64 B | 是 | C++ 靜態初始化，保留 |
| .ARM.exidx | 8 B | 是 | ARM 展開表，這版僅 8 bytes |
| .data | 1,372 B | 是 | 具初值的全域資料，保留所用物件 |
| .bss | 157,108 B | 否 | 開機清零的靜態 RAM，可查哪些物件能延後建立 |
| .xdos_xapp | 1,048,576 B | 否 | ARM32 頁表的 RAM 保留區，Loader 目前使用 320 KiB 的執行槽；調整會牽涉連結位址、頁表與核心綁定 XAPP |
| .debug_*、.comment、.ARM.attributes | ELF 資料 | 否 | 供離線診斷和工具鏈使用 |

各載入區段合計與 IMG 差 4 bytes，來自連結對齊。刪除 ELF 的除錯資訊、縮小 .bss 或 XAPP 位址保留區，都不會直接縮小已發布 IMG。

### 優化優先順序

1. **先移除 MP4 寫入端。** 目前使用 minimp4 解析影片，但連結表仍包含 MP4E_open、MP4E_put_sample 等 MP4 封裝寫入程式。先把 minimp4 實作放到獨立物件，對該物件啟用函式／資料段的未引用移除，確認讀取端、MP4 播放與配對 XAPP 都正常。約 5.9 KiB 的 mp4e_flush_index 是目前可辨識的單一候選，實際縮幅以重連結為準。
2. **收斂 LVGL 渲染格式和字型。** LVGL 在 .text 的輸入段約 600 KiB，六套 Montserrat 在 .rodata 的輸入段約 108 KiB。核心桌面以 RGB565 呈現，其他像素格式仍可能被 XAPP 圖片、透明混合或回退使用。先用輸出格式和匯入表逐項篩選，再移除未用的格式。若要減少字型，先確認 Widgets、Benchmark 及視窗資源的匯入，必要時把專用字型隨 XAPP 提供。
3. **確認 OpenVG 能否與 EGL/GLES2 分開。** Khronos client 的 vg_client.o 在 .text 約 56.8 KiB。XDOS 的桌面與影片使用 GLES2，但共用 EGL 可能引用 OpenVG。先追連結相依，再編一版移除 OpenVG，驗證 HDMI、EGL VSync 和 VSyncOpt 回退。
4. **把媒體做成選配。** MP4 Player 的 UI 已是外部 XAPP，解封裝、VCHIQ/MMAL 解碼、I420 呈現仍連在核心。如果以桌面 IMG 大小為首要目標，可讓播放器載入選配媒體模組。共用的 VC4 EGL 桌面後端應保留。USB 個別裝置驅動可另外盤點，保留主控、Hub 與板載 Ethernet。
5. **以 RAM 為目標時先量測執行期配置。** 1080p RGB565 桌面畫布每份約 3.96 MiB，兩塊 128 列 RGB565 刷新緩衝合計約 0.94 MiB，均大於 .bss 的多數小物件。VC4 還有 GPU 紋理與影片緩衝。先量開機、桌面和播放各階段，再選擇是否縮減畫布或讓媒體資源按需配置。現有 1 MiB XAPP 映射粒度獨立於 Loader 實際槽大小。

每次只調整一類，重建並比對 IMG、ELF、核心指紋與配對 XAPP。回歸至少涵蓋 Pi 3B+ 的 720p／1080p 桌面、捲動、原有 XAPP、SD／USB、Ethernet FETCH、BOOT、RESET 和回退。

## 歷史有線版

實測 IMG 為 1,434,092 bytes。下表尺寸來自同一次建置的 ELF。

| 區段 | 大小 | 用途 |
| --- | ---: | --- |
| .init | 116 B | 開機入口 |
| .text | 1,188,160 B | 核心、驅動、LVGL、VC4 程式碼 |
| .rodata | 244,368 B | 字型、鍵盤配置、常數和訊息 |
| .init_array | 64 B | C++ 初始化表 |
| .ARM.exidx | 8 B | ARM 展開表 |
| .data | 1,372 B | 初值資料 |
| .bss | 146,956 B | 靜態 RAM |
| .xdos_xapp | 1 MiB 映射，前 320 KiB 供 Loader 使用 | RAM 執行區 |
| .debug_*、.comment、.ARM.attributes | 僅在 ELF | 除錯和工具鏈資料 |

可優先試驗選配 USB 驅動（目前占用約 43.7 KiB）及 LVGL Simple／Mono 主題（約 9.1 KiB）。這些是 linker map 占用量，實際減幅需重新連結。六種 Montserrat 字號仍被 Widgets 和 Benchmark 匯入。若目標是減少 RAM，1024×768 RGB565 桌面畫布占 1.50 MiB，屬執行期配置。
