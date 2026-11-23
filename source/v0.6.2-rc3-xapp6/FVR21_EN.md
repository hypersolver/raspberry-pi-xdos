# FVR21 for Raspberry Pi 3B+

FVR21 adds the read-only UART command `@XDOS NET` on top of FVR20. It reports link readiness, DHCP or static addressing, IPv4 address, subnet mask, and default gateway. The command does not modify network settings.

If network deployment fails, reconnect over UART at 921600 baud, 8N1 and query `@XDOS NET`. When it returns `up=0` or `ip=0.0.0.0`, check Ethernet and wait for address configuration. Use a reachable address on the Windows host as `net-scp --bind`. If the Pi still has a static address from the previous LAN, review `NET.CFG` and back it up before changing it.

Use [the deployment guide](OPERATIONS_ZH.md) to install or roll back. Verify the image and the ten J7 XAPPs using SHA256SUMS.txt.

## Hardware validation

Pi 3B+ passed network transfer with CRC32, boot and reset, UART DHCP status, and host interface discovery. Rollback to FVR19 and UART PING passed. Static settings and disconnected-link behavior were not tested on hardware.
