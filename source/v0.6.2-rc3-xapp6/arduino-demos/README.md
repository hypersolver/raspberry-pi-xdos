# RPiduino Arduino LVGL Demos

兩個 sketch 各自使用標準 Arduino `setup()` 和 `loop()`，並呼叫官方 LVGL 9.4 的 Demo 入口。

| Sketch | LVGL 入口 |
| --- | --- |
| `LVGLWidgetsDemo/LVGLWidgetsDemo.ino` | `lv_demo_widgets()` |
| `LVGLBenchmarkDemo/LVGLBenchmarkDemo.ino` | `lv_demo_benchmark()` |

使用含官方 `demos/` 原始碼與圖片資源的 LVGL Library。兩個 sketch 均須啟用 `LV_USE_DEMO_WIDGETS`。Benchmark 另需 `LV_USE_DEMO_BENCHMARK`、`LV_USE_SYSMON`、`LV_USE_PERF_MONITOR` 和 `LV_FONT_MONTSERRAT_26`，並保留 14、20、24 字型。使用 1024×768 RGB565 作為 Pi 3B+ 的首輪驗收設定。Arduino 整合若只編入 `src/`，要在應用程式的建置設定另外納入官方 Demo 及 assets 的 C 檔。

板端顯示 Port 要在 `setup()` 前初始化 LVGL、毫秒 tick、input 和預設 display，並保證 LVGL 呼叫序列化。可使用 RPiduino LVGL RGB565 的 display adapter 接板端 framebuffer。若板端已在 GUI 執行上下文呼叫 `lv_timer_handler()`，建置 sketch 時加上 `RPIDUINO_LVGL_HOST_TIMER`，由板端持有事件迴圈。

XDOS XAPP5 的 WSL 建置會把這兩份 sketch 交叉編譯為與核心配對的 `LVGLDEMO.XAPP` 和 `LVGLBENCH.XAPP`。應用程式帶有官方 Demo 程式與資源，並呼叫該份核心共用的 LVGL Library。XDOS 提供執行區、顯示畫面和 GUI 生命週期。桌面執行與返回行為待 Pi 3B+ 實測。
