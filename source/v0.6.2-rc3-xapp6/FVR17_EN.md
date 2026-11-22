# FVR17 for Raspberry Pi 3B+

FVR17 resolves V2 XAPP imports at load time. The included J7 modules can run on R17-compatible kernels that export the functions and data they need. Existing kernel-bound modules still require their matching kernel. PUT (UART) and FETCH (network) accept a file already in the inbox when the filename, size and CRC32 match. A same-name file with a different size or CRC32 is rejected, preserving the original.

## Install and operate

1. Download `FVR17.IMG`, all ten `J7*.XAPP` files and `SHA256SUMS.txt`. Check the hashes with `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash -Algorithm SHA256`.
2. Use the [deployment guide](OPERATIONS_ZH.md) to configure the XDOS UART tool at 921600 baud, 8N1. Upload the IMG and the J7 files by `net-scp` under their published names, then run `boot FVR17.IMG --name FVR17.IMG` and `reset`. The revised tool handles PUT's `PRESENT` response without sending file data again.
3. For a manual microSD update, back up `config.txt` on a working XDOS FAT32 card. Copy the IMG to the root and the J7 modules to `XDOS/INBOX`, set `kernel=FVR17.IMG`, and keep the existing desktop shortcut manifests.
4. Launch desktop apps as before. For example, use `XAPP2 J7NOTE.XAPP WINDOW` to start Notepad over UART. If a name is taken by a different file, choose a fresh filename. To roll back, use BOOT and RESET to select `FVMNET2.IMG`, or restore that kernel in `config.txt`. Retain the matching N9 XAPPs.

## Pi 3B+ validation

- FVR17 and ten J7 modules passed network CRC32. The two boots showed the FVABI1 and FVR17 banners and UART PING READY.
- The same 3,228-byte `J7NOTE.XAPP` opened and released its V2 slot on both kernels without retransmission between boots.
- On FVR17, an identical PUT returned `PRESENT` without resending data. Repeating FETCH for the identical existing file succeeded.
- Both PUT and FETCH refused a same-name, same-size copy with one bit changed. The original again passed the identical FETCH check. The Files shortcut opened on FVR17.
