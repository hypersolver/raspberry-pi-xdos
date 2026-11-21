# RPiDuino Arduino SDK 0.1.2

[English](README.md)

RPiDuino 使用 Circle，把 Arduino 草稿編譯成 Raspberry Pi 可從 microSD 啟動的裸機 IMG。首版提供 `setup()`、`loop()`、數位 GPIO、ACT LED 和計時。XDOS 和 XAPP 使用各自的建置流程。

## 安裝板卡套件

1. 使用 Windows 時，先安裝 Windows 版 Python 3，並在預設 WSL Ubuntu 安裝工具鏈。Ubuntu 可直接安裝工具鏈。
2. 在 Arduino IDE 的 **檔案 → 偏好設定 → 額外的板卡管理員網址** 加入以下網址。
3. 開啟板卡管理員，搜尋 **RPiDuino** 並安裝 **0.1.2**。在 **工具 → 開發板 → RPiDuino** 選板，再從 **檔案 → 範例 → RPiDuino Basics** 開啟 Blink。

    https://raw.githubusercontent.com/hypersolver/raspberry-pi-xdos/main/package_rpiduino_index.json

Ubuntu／WSL 工具鏈安裝：

```sh
sudo apt install git make python3 gcc-arm-none-eabi libstdc++-arm-none-eabi-dev gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

首次編譯會將固定版本的 Circle 下載到 Linux 使用者快取。若已有該版本，也可在 Linux 建置環境設定 `RPIDUINO_CIRCLE_SOURCE` 指向本機 Git 儲存庫。

## Raspberry Pi 3／3B+

Pi 3B／3B+ 的 Micro‑USB 只有供電，板上的 USB-A 是主機端。使用 **Debug UART、Ethernet 或 microSD** 部署。Pi 3B+ 也可從已準備好的 USB 儲存裝置啟動。 [Raspberry Pi USB 說明](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#usb-boot-modes)

### Debug UART

用專用測試 microSD 啟動已啟用 UART 更新功能的 XDOS，鮑率為 921600，`config.txt` 需有單一生效的 `kernel=` 設定並使用 32 位元開機。原本的 XDOS 卡另行收好。Windows 在 IDE 使用的 Python 安裝 `python -m pip install pyserial`。

使用 3.3 V USB–TTL。轉接器 RX 接 Pi 實體 pin 8，TX 接 pin 10，GND 接 pin 6，VCC 留空。Arduino IDE 選 **RPiDuino 3/3B+** 和轉接器的 COM 埠，關閉序列埠監視器，按 **上傳**。工具沿用 XDOS 的 `PUT`／`DATA`／`END` 傳檔及 CRC32 校驗，再執行 `BOOT`、`RESET`。新版 IMG 使用獨立檔名，測試卡上的原核心檔仍可用於回退。

進入獨立 Blink 後，ACT LED 應亮、滅各 500 ms。此時 XDOS 更新服務已停止。若要再上傳，先關閉 Pi 電源，把測試卡上的 `xdosboot.bak` 複製回 `config.txt`，再開機回到 XDOS。

### Ethernet

先用專用測試卡啟動 XDOS，讓 Pi 與電腦接上同一個區網。把匯出的 IMG 用既有 XDOS 工具的 `net-scp` 傳到 Pi。工具透過 Debug UART 觸發 `FETCH`，IMG 本體由 Ethernet 傳送。校驗後執行 `BOOT` 和 `RESET`。 [XDOS 網路部署指令](../source/v0.6.2-rc3-xapp6/NETWORK_UPDATE_ZH.md)

### microSD

使用下方「從 microSD 啟動」的 Pi 3/3B+ 設定。

## Raspberry Pi 4B

在 IDE 選 **RPiDuino 4**。使用 USB‑C 資料線接 Windows，走下方的 **USB‑C CDC** 流程。手動 microSD 開機使用 64 位元 `kernel8-rpi4.img`。

## Raspberry Pi 5

在 IDE 選 **RPiDuino 5**。同樣走 **USB‑C CDC** 流程，接收程式會核對目標是 Pi 5。手動 microSD 開機使用 64 位元 `kernel_2712.img`。Pi 4B 和 Pi 5 的 IMG 需依各自板卡選項編譯。

## Pi 4B／5 USB‑C CDC 操作

1. 以專用測試卡啟動 64 位元 Raspberry Pi OS。安裝 `python3-serial`，並將板卡套件 `tools/cdc_receiver.py` 複製到 Pi。
2. 在測試卡的 `/boot/firmware/config.txt` 的 `[all]` 區段啟用 `dtoverlay=dwc2,dr_mode=peripheral`，確認沒有啟用 USB-C 的主機模式（`otg_mode=1`），再重開 Pi。
3. 建立測試卡標記並啟動接收程式。

   ```sh
   sudo touch /boot/firmware/RPIDUINO.TEST
   sudo modprobe g_serial
   sudo python3 cdc_receiver.py
   ```

4. 確認 Pi 有足夠供電，再用 USB‑C 資料線連接 Windows。確認 Windows 出現 USB 序列 COM 埠。安裝 `python -m pip install pyserial`，在 IDE 選對板卡及 COM 埠，再按 **上傳**。
5. 上傳工具核對機型，接收程式驗證 CRC32、將 IMG 寫入測試卡、備份設定後重開機。Blink 啟動後若要再次上傳，關機，將 `rpiduino-config.bak` 還原為 `config.txt`，移走備份檔，再開機並啟動接收程式。

CDC 接收程式僅用於有 `RPIDUINO.TEST` 標記的測試卡。實機的 USB‑C 枚舉與重開機仍待驗證。 [Raspberry Pi OTG 接線與供電說明](https://pip-assets.raspberrypi.com/categories/685-app-notes-guides-whitepapers/documents/RP-009276-WP-1-Using%20OTG%20mode%20on%20Raspberry%20Pi%20SBCs)

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
