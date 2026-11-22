# XDOS FILECLR for Raspberry Pi 3B+

Files now uses a white list and window background, with blue selection and white selected text. The Files interaction verified in FILEDBL remains in place. Single-click selects, double-click opens a folder or launches an XAPP, and the list supports wheel scrolling and a scrollbar when it overflows. The system caps directory listings at 128 entries. Deletion applies to individual files.

## Install or roll back

Download FILECLR.IMG, CLNOTE.XAPP and SHA256SUMS.txt from the release. Verify both files against the manifest using sha256sum -c SHA256SUMS.txt or PowerShell Get-FileHash -Algorithm SHA256. These are update files for an existing XDOS FAT32 microSD card. For a fresh card, first install the base system using the [installation guide](OPERATIONS_ZH.md).

- microSD update. Back up config.txt. Copy FILECLR.IMG to the card root and CLNOTE.XAPP to XDOS/INBOX. Set kernel=FILECLR.IMG in config.txt and boot.
- Network update. Use the existing UART and LAN setup at 921600 baud, 8N1. Run the guide's net-scp step for CLNOTE.XAPP and FILECLR.IMG using those exact names. Run boot for FILECLR.IMG, then reset.
- Rollback. Keep FILEDBL, FILEDEL, NOTEV2 and XAPNET images and their matching apps. Use boot with the intended rollback image and then reset. If UART is unavailable, edit kernel= on the microSD card from another computer.
- If Notepad fails to open, verify that CLNOTE.XAPP is in XDOS/INBOX and matches the release checksum.

## Pi 3B+ verification

- FILECLR was deployed over the wired network and rebooted. UART reported the FILECLR build and confirmed that OPEN FILES opened successfully.
- HDMI inspection confirmed the white background and the selected row's blue background with white text.
- Single-click selection, double-click launch, mouse wheel and deletion were verified on the preceding FILEDBL build. They were inherited by FILECLR.
