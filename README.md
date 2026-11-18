# Raspberry Pi XDOS

XD OS ARM32 移植專案。V0.1、V0.2 與 V0.3 均以 Raspberry Pi 4 Ubuntu ARM64 為建置環境，產出供 Raspberry Pi 3B／3B+ 使用的 ARM32 映像。

編譯、MicroSD 部署、實機驗證與 GitHub 發布步驟請見 [操作手冊](OPERATIONS.md)。

## V0.1 移植紀錄

V0.1 是 ARM32 靜態 framebuffer 桌面展示。建置套件以 Pi 4 Ubuntu ARM64 為建置環境，映像 xdos32.img 的目標為 Pi 3B+。套件紀錄指出 V0.1 曾在 Pi 3B+ 開機並顯示 HDMI 畫面。

## V0.2 實機紀錄

初次開機停在 Splash 畫面。MicroSD 的 `config.txt` 內將核心映像誤寫為 `xds-arm32-asm-v02.img`。補上缺少的 `o` 後，使用者確認 Pi 3B 可開機，USB 鍵盤和滑鼠正常。視窗拖曳仍會閃爍。前次分析指出兩個可能因素：直接重繪正在掃描輸出的 framebuffer，以及重繪時反覆隱藏和顯示游標。

## 重現編譯

取得 V0.2 原始碼包後，在 Raspberry Pi 4 解壓縮，進入含 `build-on-rpi4.sh` 的專案根目錄，再執行：

```bash
bash build-on-rpi4.sh
ls -lh output/*.img
sha256sum output/*.img
```

原版映像檔名為 `xdos-arm32-asm-v02.img`。若使用 DEBUG 原始碼包，對話記錄的輸出名稱為 `xdos-arm32-asm-v02-debug.img`。保留實際編譯輸出、編譯器版本及 Circle 版本，供後續重建比對。

## Raspberry Pi 3B 開機

在已有 Raspberry Pi 開機韌體的 MicroSD 開機分割區放入映像，並確認 `config.txt` 中的 `kernel=` 與檔名完全一致。以下是原版 V0.2 的設定：

```ini
[all]
arm_64bit=0
kernel=xdos-arm32-asm-v02.img
enable_uart=1
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=16
disable_splash=1
gpu_mem=64
```

## GitHub 發布準備

V0.1 與 V0.2 各自以對應的原始碼建立 Git tag（建議為 `v0.1`、`v0.2`）。將原始碼、組譯檔、建置腳本與適用的授權檔加入 repository。將對應的 `.img` 和 SHA-256 校驗值放到同版本的 GitHub Release，並在 Release 記錄標明實機驗證所用的映像版本。本機發布資料已收錄兩版原始碼與映像。

## V0.3 規劃

- 使用 RAM back buffer，減少視窗拖曳時的重繪閃爍。
- 整合 Pi 3B 有線網卡與 DHCP、DNS。
- 加入可輸入網址並顯示 HTTP 回應文字的簡易瀏覽器。HTTPS 留待後續整合。
