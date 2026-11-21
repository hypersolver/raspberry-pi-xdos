# XDOS UART and XAPP API reference

The tables describe the public PXLAYOUT prerelease. Run HELP to check the commands implemented by the image on your Pi.

## UART

Configure the Pi 3B+ serial console for 921600 baud, 8N1. Use a 3.3 V USB–TTL adapter. Connect adapter RX to Pi TX (pin 8), adapter TX to Pi RX (pin 10), and GND to GND (for example, pin 6). Power the Pi separately and leave adapter VCC disconnected.

For raw commands, prefix each line with @XDOS and a space, then terminate it with a newline. The transfer length and CRC32 fields use eight hexadecimal digits. The HTTP port is decimal.

| Firmware command | Host CLI | Purpose |
| --- | --- | --- |
| HELP | Raw UART | List commands |
| PING | ping | Check for @XDOS READY 1 |
| POWER | power | Query undervoltage status |
| LVGL | lvgl | Read the latest display performance sample |
| OPEN shortcut | open PLAYER, for example | Launch NOTEPAD, CALCULATOR, PAINT, FILES, BROWSER, TERMINAL, WIDGETS, BENCHMARK or PLAYER |
| CLOSE | close | Stop video playback or close the active XAPP |
| PLAY path | play DEMO.MP4 | Play an H.264 MP4 from SD. A bare filename is relative to XDOS/INBOX |
| BENCH-RESULT | bench-result | Read the official LVGL Benchmark scene results |
| PUT, DATA, END, CANCEL | scp | Upload a file over the serial connection |
| FETCH, FETCH-STATUS | net-scp | Download and verify a file over LAN HTTP |
| BOOT name crc32 | boot | Verify the selected IMG and update config.txt |
| BOOT-CLEAR | boot-clear | Restore the previous boot configuration |
| RESET | reset | Reboot the Pi |

The [UART client](tools/xdos-uart.py) handles argument encoding and response parsing. For a new command, register the name and argument requirements in the generated command table. Implement read-only queries in RunQuery and actions in Dispatch. Follow the existing status framing and check file or transfer state before touching the SD card.

## XAPP host interface

External applications use the AAPCS32 C ABI. The entry point is xapp_entry(const XdosHostApiV1 *api). Check abi_version and struct_bytes before accessing optional members. The XDAP file header is 32 bytes, little-endian. Keep registered callbacks and their resources valid for the lifetime of the XAPP.

| API | Members |
| --- | --- |
| XdosHostApiV1 | log, show_lvgl_demo, show_lvgl_benchmark, context and compatibility fields |
| XdosKeypadApi | open_keypad for calculator-style input |
| XdosEditorApi | open_editor with load and save callbacks |
| XdosWorkspaceApi | open_canvas, open_workspace, register_browser_fetch |
| XdosArduinoLvglApi | run_sketch with setup, loop and stop callbacks |

Workspace actions receive a zero-based button index and the current input text. PXLAYOUT supports Files, HTTP, Terminal and Player panels. The corresponding requests include LIST, LIST_USB, READ, HTTP, CANCEL_HTTP, TERMINAL and PLAY. Example entry points are available for [MP4 Player](tools/vc4/player_xapp.c) and [Terminal](tools/rc3-uart/terminal_xapp.c).

Position-independent modules can call the host table without importing kernel symbols. Modules that bind directly to kernel or LVGL symbols must be rebuilt with the matching image and fingerprint. Test loading, closing, reopening and incompatible-module handling on the target Pi.
