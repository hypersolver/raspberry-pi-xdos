# RPiDuino Arduino SDK 0.1.0

[English](README.md)

RPiDuino 使用 Circle，把 Arduino 草稿編譯成 Raspberry Pi 可從 microSD 啟動的裸機 IMG。首版提供 `setup()`、`loop()`、數位 GPIO、ACT LED 和計時。XDOS 和 XAPP 使用各自的建置流程。

## 安裝板卡套件

1. 使用 Windows 時，先安裝 Windows 版 Python 3，並在預設 WSL Ubuntu 安裝工具鏈。Ubuntu 可直接安裝工具鏈。
2. 在 Arduino IDE 的 **檔案 → 偏好設定 → 額外的板卡管理員網址** 加入以下網址。
3. 開啟板卡管理員，搜尋 **RPiDuino** 並安裝 **0.1.0**。在 **工具 → 開發板 → RPiDuino** 選板，再從 **檔案 → 範例 → RPiDuino Basics** 開啟 Blink。

    https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json

Ubuntu／WSL 工具鏈安裝：

```sh
sudo apt install git make python3 gcc-arm-none-eabi libstdc++-arm-none-eabi-dev gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

首次編譯會將固定版本的 Circle 下載到 Linux 使用者快取。若已有該版本，也可在 Linux 建置環境設定 `RPIDUINO_CIRCLE_SOURCE` 指向本機 Git 儲存庫。

## 從 microSD 啟動

在 Arduino IDE 使用 **草稿 → 匯出編譯的二進位檔**。準備一張有對應機型 Raspberry Pi 開機韌體的 FAT microSD，把匯出的 IMG 複製過去，依下表重新命名並加入兩項 `config.txt` 設定。

| 板卡 | FQBN | IMG 設定 | 架構設定 |
| --- | --- | --- | --- |
| RPiDuino 2 | `rpiduino:bcm:rpiduino2` | `kernel=kernel7.img` | `arm_64bit=0` |
| RPiDuino 3/3B+ | `rpiduino:bcm:rpiduino3` | `kernel=kernel8-32.img` | `arm_64bit=0` |
| RPiDuino 4 | `rpiduino:bcm:rpiduino4` | `kernel=kernel8-rpi4.img` | `arm_64bit=1` |
| RPiDuino 5 | `rpiduino:bcm:rpiduino5` | `kernel=kernel_2712.img` | `arm_64bit=1` |

GPIO 使用 BCM 編號 0–27。`LED_BUILTIN` 對應板上的 ACT LED。四個板卡已使用 WSL 的 Arduino CLI 編譯驗證。仍待驗證各板實機啟動與 Windows Arduino IDE 建置。

本機開發可將 `hardware/rpiduino` 複製到 Arduino Sketchbook 的 `hardware` 目錄。發布方式與 Arduino CLI 指令見英文文件。[English README](README.md)
