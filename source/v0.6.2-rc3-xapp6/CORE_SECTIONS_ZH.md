# 有線版核心區段盤點

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
