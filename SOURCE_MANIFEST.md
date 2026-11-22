# Source manifest / 原始碼清冊

Scope: Raspberry Pi 3B+ ARM32 releases through MC4FQ, the RPiDuino 0.1.4 board package, and the Circle multicore fork. This is a source inventory. Actual contents depend on the release and build flags.

範圍涵蓋 Pi 3B+ ARM32 至 MC4FQ、RPiDuino 0.1.4 板卡套件與 Circle 多核心 fork。各版實際包含的檔案以發行包及建置選項為準。

## Third-party sources / 第三方來源

| Component / 元件 | Source and revision / 來源與版本 | License and use / 授權與用途 |
| --- | --- | --- |
| Original XD OS | [eklavya-raj/xdos](https://github.com/eklavya-raj/xdos) | MIT. The ARM32 GUI port retains upstream attribution in the relevant source files. / ARM32 GUI 移植保留上游署名。[Notice / 授權全文](licenses/XDOS-MIT.txt) |
| Circle | [Circle 51.1.1 baseline, 7a81e1b](https://github.com/rsta2/circle/tree/7a81e1b480fe7d19fc1fc695b89bff0a0848f44b) | GPL-3.0-or-later for Circle code. Provides platform startup, multicore, interrupts, USB, Ethernet, SD, and other platform services. Imported addons retain their own notices. / 提供開機、多核心、中斷、USB、有線網路和 SD 等服務，匯入的 addon 依各自授權。[GPLv3 text](licenses/Circle-GPL-3.0.txt) |
| Circle multicore fork | [hypersolver/circle, 8575ec52](https://github.com/hypersolver/circle/commit/8575ec526827780a2cb77238b51168c2a053512e) | GPL-3.0-or-later for the modified Circle file. The project added cross-core serialization around the property-tag transaction. MC4FQ uses the equivalent patch through its own build chain. / 自研 mailbox 跨核心鎖沿用 Circle 原檔授權。MC4FQ 由原建置鏈套用等效修改。 |
| LVGL 9.4 | [c016f72d](https://github.com/lvgl/lvgl/tree/c016f72d4c125098287be5e83c0f1abed4706ee5) | MIT for LVGL core, with additional notices for imported libraries, assets and fonts. Used by the desktop rendering path, official Widgets and Benchmark XAPPs, and SDK headers. / 用於桌面繪圖、官方 Widgets、Benchmark 與 SDK 標頭。附帶庫、圖資與字型另查原檔通知。[LVGL license](source/v0.6.2-rc3-xapp6/LVGL-LICENSE.txt) |
| Montserrat | [LVGL bundled font notice](https://github.com/lvgl/lvgl/blob/c016f72d4c125098287be5e83c0f1abed4706ee5/scripts/built_in_font/font_license/Montserrat/OFL.txt) | OFL-1.1. Used where enabled by the LVGL build configuration. / 依 LVGL 建置選項啟用。 |
| FatFs | [ChaN FatFs](https://elm-chan.org/fsw/ff/), R0.16 in the MC4FQ prepared source | FatFs license from the vendored header and license file. Provides filesystem services for SD and USB mass storage. / SD 和 USB 隨身碟的檔案系統元件，授權見所附標頭和授權檔。 |
| Broadcom VideoCore user interface and Circle VC4 addons | Prepared Circle source, addon/vc4 and addon/linux | Mixed file-level notices. Broadcom EGL client carries a BSD-style notice. Retain the notices for the VCHIQ, EGL/GLES2 and audio files used by the build, including Linux compatibility sources with their own license terms. / VCHIQ、EGL/GLES2、音訊及 Linux 相容程式按檔案保留各自授權。 |
| Pi1MHz | [1f6954ee](https://github.com/dp111/Pi1MHz/tree/1f6954eee2d1f72eab7e187e0a2d941763a54345), documented PXLAYOUT source pin | The imported decoder files retain their GPL notices. Used for the VideoCore H.264 integration. / 用於 VideoCore H.264 整合，保留匯入檔案的 GPL 通知。 |
| minimp4 | [lieff/minimp4](https://github.com/lieff/minimp4), PXLAYOUT pin 65fab0a062c3cd5befd98ecbaa890108cd45c226 | CC0-1.0 notice in the header. MP4 container handling. / 標頭有 CC0-1.0 通知，負責 MP4 容器處理。 |
| minimp3 | [lieff/minimp3](https://github.com/lieff/minimp3), vendored header in MC4FQ prepared source | CC0-1.0 notice in the header. Software MP3 decoding. / 標頭有 CC0-1.0 通知，負責軟體 MP3 解碼。上游 commit 尚未核對，檔案指紋如下。 |
| Raspberry Pi boot firmware | [raspberrypi/firmware boot files](https://github.com/raspberrypi/firmware/tree/master/boot) | Broadcom redistribution terms for covered firmware binaries. Track the exact revision and file notices when bundling boot files. MC4FQ is an upgrade overlay for an existing FILEQ128 card. / 韌體二進位檔依 Broadcom 條款，打包時記錄確切版本。MC4FQ 為既有 FILEQ128 卡的升級包。[Broadcom terms](https://github.com/raspberrypi/firmware/blob/master/boot/LICENCE.broadcom) |
| WLAN, hostap and wireless firmware | Circle WLAN addon and separately sourced firmware | Legacy or optional. Inspect component-level notices and firmware terms independently. Wired MC4FQ still has WLAN source files in its prepared source archive. / 舊版或選配元件，各自保留授權。MC4FQ 有線版的建置來源壓縮檔仍收錄 WLAN 原始檔。 |

MC4FQ prepared source header SHA-256 / MC4FQ 建置來源標頭 SHA-256

- sample/xdos-sdboot/minimp3.h — bdfe16be3efee63acb98a67ea770f8051eeefe4fd53a5156bf98bdaba4eff0ab
- sample/xdos-sdboot/minimp4.h — 1a21f99afa15815e0e51eba416272f60886f7a72ebf15c36f884b0d42e2c8580

## Project work / 自研部分

| Area / 範圍 | Project contribution / 自研內容 |
| --- | --- |
| XDOS kernel and desktop | ARM32 integration, boot orchestration, desktop layout and manifest scanning, window/input behavior, storage and update services, shared UART/Terminal command registry. / ARM32 整合、開機流程、桌面配置與清單掃描、視窗與輸入、儲存與更新、UART／Terminal 指令表。 |
| XAPP runtime | Loader, host ABI, V2 relocation and execution slots, LVGL core borrowing and idle accounting, app packaging and examples. / 載入器、主系統 ABI、V2 重定位與執行槽、LVGL 核心借用與閒置統計、打包及範例。 |
| Display and media integration | RGB565 display integration, VC4 presentation and texture caching, playback controls and host service integration. / RGB565 顯示整合、VC4 顯示與紋理快取、播放控制和主系統服務。 |
| RPiDuino SDK and developer tools | Board definitions, Arduino runtime glue, linker and upload scripts, examples, build and release automation. Bundled LVGL and FatFs headers keep their upstream notices. / 板卡定義、Arduino 執行整合、連結與上傳、範例、建置與發行工具。附帶標頭保留上游授權。[SDK tree](Arduino%20SDK/) |

Original project files have no separately declared universal license in the current repository. Upstream-derived files retain their notices; for linked distributions preserve the applicable license and corresponding-source materials. See [license information / 授權資訊](LICENSE.md).

目前專案自研檔案尚未另外指定統一授權。衍生檔保留上游通知，散布連結後的版本時應一併保留適用的授權和相應原始碼資料。

## Release provenance / 發行來源

- [MC4FQ](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-mc4fq): prepared Circle tree, source patches, build information, checksums and paired XAPP assets. / 包含建置前 Circle 樹、補丁、建置資訊、雜湊和配對 XAPP。
- [FILEQ128 and RPiDuino 0.1.4](https://github.com/hypersolver/raspberry-pi-xdos/releases/tag/v0.6.2-rc3-xapp6-fileq128): baseline upgrade/install material and SDK package. / 基礎升級及安裝資料、SDK 套件。
- [PXLAYOUT source recipe](source/v0.6.2-rc3-xapp6/PXLAYOUT_SOURCE.md): media and display dependency pins. / 影音和顯示依賴版本。
- Build tools such as GNU Arm GCC, GNU binutils, Python, WSL and Arduino IDE are host prerequisites. Their own distributions carry their respective terms. Verify linked runtime objects and any redistributed tools separately. / 主機建置工具依各發行版授權；另核對實際連結的執行期物件和隨包散布的工具。
- Record each future release's upstream pins or file hashes, configuration, patches, executable and XAPP checksums, and license notices. Independently record attribution and redistribution terms for sample audio, video, photos and other assets before bundling them. Historical releases and the full optional-addon inventory still require release-by-release verification. / 後續發行記錄上游版本或檔案雜湊、組態、補丁、映像和 XAPP 校驗及授權通知。範例影音及圖片打包前另記來源與散布條款。舊版本和所有選配模組仍須逐包核對。
