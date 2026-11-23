# XDOS configuration · 設定說明 · 設定ガイド

[English](#english) · [繁體中文](#繁體中文) · [日本語](#日本語) · [Configuration examples](#configuration-examples--設定範例--設定例)

## English

| File on microSD | Purpose |
| --- | --- |
| `config.txt` at the root | Raspberry Pi firmware settings. `kernel=FVR21.IMG` selects the XDOS kernel at the next boot. Keep a copy before changing it. |
| `XDOS/XDOS.CFG` | XDOS work mode and application startup. XDOS tries `XDOS/INBOX/XDOS.CFG` when it cannot open the primary file. A staged copy never overrides a readable primary file. |
| `XDOS/INBOX/NET.CFG` | Static wired IPv4 configuration. Without a valid file, XDOS uses DHCP. |
| `XDOS/LANG/<CODE>.TXT`, `XDOS/FONTS/` | Runtime language dictionaries and fonts in the FVLANG1 test build. The released FVR21 does not load these files. |

Use plain text. For `NET.CFG`, use ASCII or UTF-8 without BOM, one `KEY=VALUE` per line without spaces around `=`, and supply each of the four keys once. Blank lines and lines starting with `#` are allowed. Choose an unused or reserved address appropriate for the LAN. Back up and rename `NET.CFG`, then reboot to return to DHCP. On FVR21, `@XDOS NET` over UART at 921600 baud, 8N1 reports the current IPv4 address, mask, gateway and addressing method. It does not renew a lease or edit settings.

In `XDOS.CFG`, `MODE=DESKTOP` is the normal GUI. `MODE=IOT` suspends LVGL refresh and permits background XAPPs with `Screen=None`. `MODE=KIOSK` opens the specified fullscreen XAPP. `KIOSK_EXIT` defaults to `RESTART` and can also be `DESKTOP` or `REBOOT`. In FVR21, `AUTO` selects KIOSK when `KIOSK=` is populated and DESKTOP otherwise. It does not inspect HDMI. `AUTORUN` is a comma-separated list of up to four background XAPPs across modes, subject to available slots. Relative XAPP names resolve in INBOX. A full `SD:/...` path is also accepted. `#` and `;` start comments. Use `@XDOS MODE` to query the current mode. FVR21 allows runtime switching between DESKTOP and KIOSK. Change the file and reboot to enter or leave IOT.

On FVLANG1, set `LANG=ZH-TW` in the effective `XDOS.CFG`. The UTF-8 dictionary maps keys to labels, including `APP.<original name>` for shortcuts. XDOS tries `XDOS/INBOX/<CODE>.TXT` if the language-directory copy cannot be opened. Use `FONT=16` with `XDOS-TC16.bin` or `FONT=20` with `XDOS-TC20.bin`, and keep the font license alongside the assets. `@XDOS LANG` queries the language and `@XDOS LANG EN` switches to built-in English during the current session. Edit `XDOS.CFG` for a persistent choice. Japanese documentation is available. A Japanese runtime dictionary and compatible glyph coverage have not been validated.

## 繁體中文

| microSD 檔案 | 用途 |
| --- | --- |
| 根目錄的 `config.txt` | Pi 韌體設定。`kernel=FVR21.IMG` 指定下次開機用的核心，修改前先備份。 |
| `XDOS/XDOS.CFG` | 開機模式與 XAPP 啟動參數。主檔無法開啟時讀 `XDOS/INBOX/XDOS.CFG`。只上傳 INBOX 副本無法覆蓋可讀取的主檔。 |
| `XDOS/INBOX/NET.CFG` | 有線固定 IPv4。沒有有效設定檔時採用 DHCP。 |
| `XDOS/LANG/<CODE>.TXT`、`XDOS/FONTS/` | FVLANG1 測試版的執行時語系字典及字型。FVR21 不會載入這些檔案。 |

`NET.CFG` 使用 ASCII 或無 BOM 的 UTF-8 純文字。每行一組 `KEY=VALUE`，等號前後不加空白，四個欄位各填一次。可使用空行及以 `#` 開頭的註解。固定 IP 請選擇所在網段未使用或已保留的位址。要恢復 DHCP，先備份並改名，再重開。FVR21 可用 921600 baud、8N1 的 UART 指令 `@XDOS NET` 查詢目前 IP、遮罩、閘道和取址方式。查詢不會更新租約或寫入設定。

`MODE=DESKTOP` 是一般 GUI。`MODE=IOT` 暫停 LVGL 更新，只接受 `Screen=None` 背景 XAPP。`MODE=KIOSK` 啟動指定的全螢幕 XAPP。`KIOSK_EXIT` 預設 `RESTART`，也可設定 `DESKTOP` 或 `REBOOT`。FVR21 的 `AUTO` 有填 `KIOSK=` 就選 KIOSK，否則進桌面，與 HDMI 是否連接無關。`AUTORUN` 可列最多四個以逗號分隔的背景 XAPP，各模式均會處理，仍受可用槽位限制。相對檔名在 INBOX 查找，也可填完整 `SD:/...` 路徑。`#` 和 `;` 可用於註解。`@XDOS MODE` 查詢模式，FVR21 可在執行中切換 DESKTOP／KIOSK；進出 IOT 需修改設定後重開。

FVLANG1 可在生效的 `XDOS.CFG` 加上 `LANG=ZH-TW`。UTF-8 語系檔的每行把鍵對應到顯示文字，例如 `APP.<原名稱>` 用於捷徑。語系目錄的檔案無法開啟時，會查 `XDOS/INBOX/<CODE>.TXT`。`FONT=16` 使用 `XDOS-TC16.bin`，`FONT=20` 使用 `XDOS-TC20.bin`，部署時請一起保留字型授權。`@XDOS LANG` 查詢語系，`@XDOS LANG EN` 在本次執行期間切回內建英文。要跨重開保留，請改設定檔。日文目前提供文件，日文執行時字典與字型字集尚未驗證。

## 日本語

| microSD 上のファイル | 用途 |
| --- | --- |
| ルートの `config.txt` | Pi の起動設定。`kernel=FVR21.IMG` を指定すると次回起動時に適用されます。変更前にバックアップしてください。 |
| `XDOS/XDOS.CFG` | 起動モードと XAPP 設定。主ファイルを開けない場合は `XDOS/INBOX/XDOS.CFG` を参照します。 |
| `XDOS/INBOX/NET.CFG` | 有線の固定 IPv4 設定。有効な設定がなければ DHCP を使用します。 |
| `XDOS/LANG/<CODE>.TXT`、`XDOS/FONTS/` | FVLANG1 試験版の言語辞書とフォント。FVR21 では読み込みません。 |

`NET.CFG` は ASCII または BOM なし UTF-8 で記述します。`KEY=VALUE` の前後に空白を入れず、4 項目を一度ずつ記載します。空行と `#` で始まるコメントは使用できます。DHCP に戻すには設定をバックアップして名前を変更し、再起動します。FVR21 では UART（921600 baud、8N1）の `@XDOS NET` で現在のアドレス、マスク、ゲートウェイ、設定方式を確認できます。このコマンドはリースの更新や設定変更を行いません。

`DESKTOP` は通常の GUI、`IOT` は LVGL 更新を停止し `Screen=None` のバックグラウンド XAPP を実行するモードです。`KIOSK` は指定アプリを全画面で起動し、終了時に `KIOSK_EXIT` の `RESTART`（既定）、`DESKTOP`、`REBOOT` のいずれかを適用します。FVR21 の `AUTO` は KIOSK の指定があれば KIOSK、なければ DESKTOP を選びます。HDMI は判定に使いません。`AUTORUN` は最大 4 件をカンマ区切りで指定でき、空きスロットに依存します。相対ファイル名は INBOX を参照し、`SD:/...` の絶対パスも使えます。`@XDOS MODE` で状態を確認できます。DESKTOP と KIOSK は実行中に切り替えられます。IOT への切り替えは設定を変更して再起動してください。

FVLANG1 では有効な設定ファイルに `LANG=ZH-TW` を指定します。UTF-8 辞書は `APP.<元の名前>` などのキーを表示ラベルに対応させます。既定の場所から読み込めない場合は INBOX を参照します。`FONT=16` または `FONT=20` に対応する繁体字フォントとライセンスを配置してください。`@XDOS LANG` で言語を確認し、`@XDOS LANG EN` でセッション中に英語に戻せます。永続化は設定ファイルで行います。日本語の実行時辞書とフォントは実機未検証です。

## Configuration examples · 設定範例 · 設定例

**Static IPv4** — `XDOS/INBOX/NET.CFG`. Replace these documentation-only addresses before use.

~~~ini
IP=192.0.2.20
MASK=255.255.255.0
GATEWAY=192.0.2.1
DNS=192.0.2.1
~~~

**GUI** — `XDOS/XDOS.CFG`

~~~ini
MODE=DESKTOP
~~~

**IOT** — Install a background-capable `LOGGER.XAPP` in INBOX before enabling it.

~~~ini
MODE=IOT
AUTORUN=LOGGER.XAPP
~~~

**KIOSK** — Install a fullscreen-capable `PANEL.XAPP` in INBOX before enabling it.

~~~ini
MODE=KIOSK
KIOSK=PANEL.XAPP
KIOSK_EXIT=DESKTOP
~~~

**Runtime language** — FVLANG1 test build only. Add `LANG=ZH-TW` to `XDOS.CFG`, then use the following dictionary and font layout.

~~~text
XDOS/
  XDOS.CFG
  LANG/
    ZH-TW.TXT
  FONTS/
    XDOS-TC16.bin
    OFL.txt
~~~

~~~ini
# XDOS/LANG/ZH-TW.TXT
FONT=16
APP.Terminal=終端機
APP.Calculator=計算機
~~~

[Installation and rollback](flashing-by-board.md) · [FVR21 network query](../source/v0.6.2-rc3-xapp6/FVR21_EN.md) · [XDOS operations](../source/v0.6.2-rc3-xapp6/OPERATIONS_ZH.md)
