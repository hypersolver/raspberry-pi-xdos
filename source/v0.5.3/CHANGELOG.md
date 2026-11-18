# CHANGELOG

## V0.5.3-RC (RPi 3B / 3B+)
- Default terminal/notepad windows closed; no window focused at startup.
- Ethernet PHY probe, SDIO Wi-Fi firmware, WPA2 and DHCP are deferred until the desktop becomes visible.
- One-shot Circle cooperative CNetworkInitTask starts after first desktop rendering.
- Keep prior GUI assembly routines, USB keyboard/mouse, browser timeout and Ethernet/Wi-Fi one-image fallback.
- No binary in ZIP; build on RPi4 Ubuntu ARM64.


建置環境為 Raspberry Pi 4 Ubuntu ARM64。BUILD_USER、BUILD_HOST 與 /path/to/xdos-build 為佔位符。Wi-Fi 連線設定需在裝置端建立。
