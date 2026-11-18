# Run after building on RPi 4 (Windows PowerShell).
$dst = '.\xdos32-v053.img'
scp BUILD_USER@BUILD_HOST:/path/to/xdos-build/XDOS_ARM32_ASM_V0.5_3_DESKTOP_FIRST_RPi3B/output/xdos32-eth-wifi.img $dst
Write-Host "Copy $dst to your Pi 3B/3B+ microSD and set kernel=xdos32-v053.img"
