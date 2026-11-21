# XDOS 區網部署與回退

目前公開版請依 [PXLAYOUT 指南](PXLAYOUT_ZH.md#既有-xdos-透過-ethernet-更新) 的檔名與命令操作。以下 HDBENCH 範例用於早期 HDMI Auto 測試版。各版本均須使用各自配對的核心和 XAPP。

需要已安裝並支援 `FETCH`、`BOOT`、`RESET` 的 XDOS。將 Pi 3B+ 與電腦接到同一區網，再接 3.3 V UART。UART 傳送命令，檔案由 Pi 經 Ethernet 下載。首次安裝可把 Release 測試包解壓到已備妥 Raspberry Pi 開機韌體的 FAT 分割區，然後在 `config.txt` 的通用區段設定 `kernel=HDBENCH.IMG`，保留原 IMG 與設定備份。

1. 下載同一個 Release 的 IMG、三個 XAPP 和 `SHA256SUMS.txt`。用 PowerShell 的 `Get-FileHash -Algorithm SHA256 <檔案>` 核對。
2. 在專案版本目錄執行以下命令。把套件路徑和 COM 埠換成實際值，輸入電腦可供 Pi 存取的區網 IPv4。若 Windows 防火牆詢問，允許 Python 在目前的私人區網接收入站連線。預設 HTTP 埠是 8765。

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install pyserial
$python = '.\.venv\Scripts\python.exe'
$uart = @('tools/xdos-uart.py', '--port', 'COM4', '--baud', '921600')
$assets = '下載附件的目錄'
$hostIp = Read-Host '電腦的區網 IPv4'
& $python @uart ping
if ($LASTEXITCODE) { throw 'UART 無回應' }
$files = [ordered]@{
    'BROWSER.XAPP'   = 'HBBROWSE.XAPP'
    'LVGLDEMO.XAPP'  = 'HBWIDGET.XAPP'
    'LVGLBENCH.XAPP' = 'HBBENCH.XAPP'
    'HDBENCH.IMG'   = 'HDBENCH.IMG'
}
foreach ($file in $files.GetEnumerator()) {
    & $python @uart net-scp (Join-Path $assets $file.Key) --bind $hostIp --name $file.Value
    if ($LASTEXITCODE) { throw "傳送失敗：$($file.Key)" }
}
& $python @uart boot (Join-Path $assets 'HDBENCH.IMG') --name HDBENCH.IMG
if ($LASTEXITCODE) { throw '設定開機映像失敗' }
& $python @uart reset
```

每個檔案均由 Pi 核對長度與 CRC32。目的檔已有同名檔時，`FETCH` 會拒絕寫入。若前一次已確認某個檔案傳輸成功，可以略過該檔繼續，原有檔案須與本次 Release 配對。若同名檔屬於別的建置，先在 microSD 上備份並移出其舊檔。請保留原核心和配對 XAPP 供回退。

3. 重啟後確認 UART 可 `PING`。開機訊息 `[DISPLAY]` 會列出 framebuffer、桌面、LVGL、VC4 和 HDMI 的尺寸。確認桌面及各 XAPP 正常。
4. 要回退，先執行 `boot-clear`，成功後執行 `reset`。若 UART 無法連線，關閉 Pi 並在電腦上掛載 microSD 開機分割區，還原備份的 `config.txt`，或將 `kernel=` 指回原核心。

完整指令及新增命令方式見 [UART 指令與開發指南](UART_ZH.md)。
