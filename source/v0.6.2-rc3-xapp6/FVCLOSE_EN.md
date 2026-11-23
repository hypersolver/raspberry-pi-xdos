# FVCLOSE for Raspberry Pi 3B+

Based on FVR21. UART CLOSE first tries to close the active fullscreen app. On the desktop it can close the focused V2 window. XAPP2-CLOSE can target a known slot directly.

Deploy FVCLOSE.IMG and verify SHA-256 using the [installation guide](OPERATIONS_ZH.md). The J7 modules are compatible with FVR21. Use OPEN CALCULATOR or OPEN BENCHMARK and then CLOSE over UART. Check XAPP2-STATUS for the released slot. To roll back, use BOOT to select the preserved image, RESET and PING.

On Pi 3B+, network CRC32 passed. CLOSE released both the Calculator window and fullscreen Benchmark slots. Rollback to FVR19 restored UART READY.
