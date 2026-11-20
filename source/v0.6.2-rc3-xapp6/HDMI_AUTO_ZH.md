# XDOS V0.6.2-RC3 XAPP6 HDMI Auto

Pi 3B+ ARM32 裸機有線版。XDOS 在開機時向韌體取得 HDMI 模式，並以實際 RGB565 framebuffer 尺寸初始化桌面、LVGL 與 VC4。若自動模式無法配置，會以 1024×768 重試。換螢幕後重新開機即可再次偵測。指定 HDMI 模式時，韌體會依開機設定選擇輸出。

| 解析度 | 驗證狀態 |
| --- | --- |
| 1024×768 | HDMI Auto 版已確認各層尺寸一致，桌面目視正常。官方捲動場景可沿用已驗證的 VC4 紋理快取。 |
| 1280×720 | HDMI 模式驗證版已確認各層尺寸一致，桌面目視正常。此 Release 的 720p Benchmark 尚未重跑。 |
| 1920×1080 | 此 Release 已確認各層尺寸一致，跑完整官方 Benchmark，卡片上捲目視無殘影。 |

官方 Benchmark 的 `Containers with scrolling` 在寬度超過 1024 或高度超過 768 時使用 LVGL 重繪，VC4 仍以 EGL VSync 呈現。這項保護也適用於 720p。其他解析度能否使用，以及是否需要保護，仍需逐一驗證。這個限制只作用於官方 Benchmark 的該場景。

1080p 實測捲動場景為 **12 FPS、Render 19 ms、Flush 51 ms**，全場景平均 18 FPS。測試期間 VC4 啟用，LVGL 刷新週期為 20 ms。完整資料見 `benchmark-1080p.csv`。

## 安裝、操作與建置

- 首次安裝可使用 SD 測試包，其中附核心與配對桌面應用。放入已具 Raspberry Pi 開機韌體的 FAT 開機分割區，備份並設定 `config.txt` 的 `kernel=HDBENCH.IMG`。
- 已安裝 XDOS 的裝置可依 [網路部署與回退](NETWORK_UPDATE_ZH.md) 更新。UART 指令與新增命令請見 [UART 指令與開發指南](UART_ZH.md)。
- 從版本來源目錄在 WSL 執行下列命令。核心與外部 XAPP 應取自同次建置。

```bash
XDOS_HDMI_BENCH_TEST=1 XDOS_WIRED_ONLY=1 XDOS_ANALYTICS_MODE=analyticsregion XDOS_BROWSER_XAPP=1 XDOS_TEST_DEBUG=0 bash tools/build-wsl.sh "$PWD"
```
