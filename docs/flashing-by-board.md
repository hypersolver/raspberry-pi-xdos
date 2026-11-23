# XDOS installation and live updates by Raspberry Pi model

[English](#english) · [繁體中文](#繁體中文) · [日本語](#日本語)

## English

| Board | First installation | While XDOS is running |
| --- | --- | --- |
| Raspberry Pi 3B / 3B+ | Prepare a microSD with the Pi boot firmware and matching XDOS files. The micro-USB connector provides power. | GPIO Debug UART or wired Ethernet. Pi 3B+ is the verified target for FVR21. |
| Raspberry Pi 4 Model B | Use boot media prepared for the target board. USB-C supports USB device mode with suitable firmware or software. | A direct USB-C update would require a USB gadget and an XDOS receiver on that board. This path is not verified in the FVR21 release. |
| Raspberry Pi 5 | Use boot media and an image built for Pi 5. USB-C supports USB device mode with suitable firmware or software. | Direct USB-C updates likewise require board-specific support and validation. |

**Pi 3B/3B+ wiring.** Connect a 3.3 V USB-to-TTL adapter to the PC. Cross TX and RX as shown, connect ground, and leave the adapter power pin disconnected. Power the Pi separately. For network delivery, keep UART connected for control and connect the Pi and PC to the same LAN.

**Live delivery.** On FVR21 query `@XDOS NET` over UART to confirm the Pi address. Use `net-scp` with a reachable host address, or UART `scp`. A kernel IMG is verified by CRC32, selected with BOOT, then activated with RESET. An Arduino XAPP can be uploaded while XDOS runs and launched without a system reset. Back up the existing boot configuration and keep the previous IMG for rollback.

## 繁體中文

| 板型 | 首次安裝 | XDOS 執行中更新 |
| --- | --- | --- |
| Raspberry Pi 3B／3B+ | 用讀卡機準備 microSD，放入 Pi 開機韌體和配對的 XDOS 檔案。板上的 Micro-USB 用於供電。 | 用 GPIO Debug UART 或有線網路傳檔。FVR21 以 Pi 3B+ 實機驗證。 |
| Raspberry Pi 4 Model B | 使用該板型的開機媒體。USB-C 在適當韌體或軟體下支援 USB 裝置模式。 | USB-C 直連需要板上 USB gadget 和 XDOS 接收端。FVR21 尚未驗證這條路徑。 |
| Raspberry Pi 5 | 使用 Pi 5 專用建置與開機媒體。USB-C 在適當韌體或軟體下支援 USB 裝置模式。 | USB-C 線上傳檔及燒錄需另外實作、實測。 |

**Pi 3B／3B+ 接線。** 電腦接 3.3 V USB 轉 TTL，TX 和 RX 交叉，GND 共地，轉接器 VCC 不接。Pi 獨立供電。走區網時 UART 保留供查詢及控制，電腦與 Pi 接到可互通的區網。

**執行中部署。** FVR21 用 UART 的 `@XDOS NET` 查現行 IP，再用 `net-scp` 選擇 Pi 可到達的電腦位址，或用 UART `scp`。IMG 經 CRC32 校驗後用 BOOT 選取，RESET 後才啟用。Arduino XAPP 可在 XDOS 執行中上傳，然後啟動，無須重開系統。部署前備份開機設定並保留回退 IMG。

## 日本語

| 機種 | 初回インストール | XDOS 実行中の更新 |
| --- | --- | --- |
| Raspberry Pi 3B／3B+ | 起動ファームウェアと対応する XDOS ファイルを microSD に配置。Micro USB は電源用です。 | GPIO デバッグ UART または有線 LAN。FVR21 の実機確認は Pi 3B+ で実施しました。 |
| Raspberry Pi 4 Model B | 対応する起動メディアを用意。USB-C は対応するファームウェアまたはソフトウェアでデバイスモードを利用できます。 | USB-C 直結には USB gadget と XDOS 側の受信機能が必要です。FVR21 では実機未検証です。 |
| Raspberry Pi 5 | Pi 5 用のビルドと起動メディアを使用。USB-C のデバイスモードに対応します。 | USB-C 直結による実行中の更新は、機種別の実装と検証が必要です。 |

**Pi 3B／3B+ の配線。** 3.3 V USB-TTL アダプターを使い、TX と RX を交差させ、GND を共通にします。VCC は接続せず、Pi は別途給電します。有線転送の場合も、UART は状態確認と制御に使用できます。

**実行中の更新。** FVR21 では UART の `@XDOS NET` で現在の IP を確認します。到達可能なホストアドレスを `net-scp` に指定するか、UART の `scp` を使用します。IMG は CRC32 を確認して BOOT で選択し、RESET 後に有効になります。Arduino XAPP の転送と起動にはシステムの再起動は不要です。更新前に起動設定をバックアップし、旧 IMG を保管します。

## Wiring · 接線 · 配線

~~~mermaid
flowchart LR
    PC["PC"] -->|"USB data"| A["3.3 V USB–TTL"]
    A -->|"TX → pin 10 / GPIO15 RX"| P3["Pi 3B / 3B+"]
    P3 -->|"pin 8 / GPIO14 TX → RX"| A
    A ---|"GND ↔ pin 6; VCC disconnected"| P3
    PSU["Separate Pi power supply"] -->|"Micro USB power"| P3
    PC <-->|"Ethernet via LAN"| P3
    PC -->|"USB-C data cable"| P45["Pi 4 / Pi 5 USB-C device port"]
    P45 -.-> G["Compatible USB gadget + update receiver required"]
~~~

## Flashing paths · 燒錄路徑 · 書き込みフロー

~~~mermaid
flowchart TD
    B{"Deployment path"} -->|"Pi 3B / 3B+ first install or recovery"| SD["PC card reader → FAT boot partition → Pi firmware + IMG + config.txt"]
    B -->|"Pi 3B / 3B+ runtime"| LINK["GPIO UART PUT or Ethernet FETCH"]
    LINK --> C["microSD / XDOS / INBOX → CRC32"]
    C -->|"Kernel IMG"| K["BOOT → RESET → PING"]
    C -->|"Arduino XAPP"| A["Install .APP if needed → launch"]
    B -.->|"Pi 4 / Pi 5 runtime USB-C · unverified in XDOS"| USB["USB device enumeration → gadget transport → compatible update receiver"]
    USB -.-> C
~~~

For a Pi 4 / Pi 5 USB connection, use a data-capable cable and a power arrangement suitable for the board. A USB device connection under another OS does not establish that XDOS can accept an update through it.

[XDOS operations](../source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md) · [UART reference](../source/v0.6.2-rc3-xapp6/UART_ZH.md) · [FVR21 network query](../source/v0.6.2-rc3-xapp6/FVR21_EN.md)

[Raspberry Pi OTG application note](https://pip-assets.raspberrypi.com/categories/685-app-notes-guides-whitepapers/documents/RP-009276-WP-1-Using%20OTG%20mode%20on%20Raspberry%20Pi%20SBCs) · [Raspberry Pi UART pin assignments](https://www.raspberrypi.com/documentation/computers/configuration.html#primary-and-secondary-uarts)
