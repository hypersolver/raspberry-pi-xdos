# XD OS ARM32 V0.3 — Circle Ethernet + HTTP Browser + reduced-flicker desktop

**Host:** Raspberry Pi 4, Ubuntu ARM64.
**Target:** Raspberry Pi 3B (BCM2837 Cortex-A53, AArch32 bare metal).

This package is a source integration release; it is not an RPi3B-tested binary.
It must be linked against **your working Circle checkout**. Keep the confirmed-working
V0.2 image on SD card for rollback.

## What's actually included

- Original V0.2 AArch32 `xdos_gui.S`: rectangle, glyph, hit test and drag logic.
- RAM back buffer: the full scene is drawn offscreen, then copied onto the HDMI framebuffer.
- Drag-damage bounding rectangle: only the moved window's old/new screen region is blitted.
- One software cursor overlay: Circle's standard framebuffer cursor is disabled to avoid
  repeated hide/show flicker; cursor movement only updates a small area.
- Combined USB keyboard and mouse through Circle, previously independently tested.
- Pi3B LAN9514 via Circle's SMSC951x USB Ethernet driver.
- `CNetSubSystem` DHCP, IPv4, DNS, TCP, HTTP client and a cooperative Circle CWebTask.
- GUI HTTP text browser: URL input, GET, rudimentary HTML-to-text rendering.
- Terminal commands `help`, `net`, `browse http://<host>/`, plus previous commands.

**Limitations:** No HTTPS/TLS, CSS, JavaScript, images, HTML links, redirects or arbitrary
ports. HTTP 200 responses capped to 48 KiB; displayed text limited to 12 KiB. Real
websites often use HTTPS, so start with your own local HTTP test server. Network
initialization is nonblocking (desktop still boots without Ethernet cable). A RAM back
buffer reduces visible incremental painting but is not an actual vsynced page flip.

## Build on Raspberry Pi 4 Ubuntu ARM64

```bash
cd ~/xdos-build
# Transfer this ZIP from Windows with scp and extract it first.
unzip -o XDOS_ARM32_ASM_V0.3_NET_Browser_RPi3B.zip
cd XDOS_ARM32_ASM_V0.3_NET_Browser_RPi3B
bash build-on-rpi4.sh
ls -lh output/xdos32-v03.img
```

The script requires `~/xdos-build/circle` configured with `RASPPI = 3`, `AARCH = 32`
(as already verified) and the Circle libraries already built (`./makeall`).
On compile/link error, send the entire build output; the exact Circle checkout commit
may differ from this source's API baseline.

## Transfer to Windows

Run in **Windows PowerShell**, *not* inside RPi4 SSH:

```powershell
scp BUILD_USER@BUILD_HOST:/path/to/xdos-build/XDOS_ARM32_ASM_V0.3_NET_Browser_RPi3B/output/xdos32-v03.img ".\"
```

Copy `xdos32-v03.img` from Windows Downloads to the RPi3B MicroSD FAT32 root.
In `config.txt` change ONLY the kernel line (keep known-good boot firmware/settings):

```ini
[all]
arm_64bit=0
kernel=xdos32-v03.img
```

**Rollback:** `kernel=xdos-arm32-asm-v02-debug.img` (using your actual known-good filename).

## Hardware test sequence

1. Boot with HDMI + USB keyboard + mouse + wired Ethernet to a DHCP router.
2. Check desktop shows mouse/keyboard status; drag windows and observe flicker.
3. After DHCP finishes, top right should show local IP; Terminal `net` reports it.
4. Open Browser (W taskbar icon); click URL bar; type HTTP URL; press Enter.
5. Test a small local HTML page on RPi4 as server:

```bash
mkdir -p ~/xdos-build/webtest
printf '<html><body><h1>XD OS NETWORK OK</h1><p>Pi3B HTTP browser test.</p></body></html>\n' > ~/xdos-build/webtest/index.html
# On Pi4; allow TCP/80 through any configured firewall.
sudo python3 -m http.server 80 --bind 0.0.0.0 -d ~/xdos-build/webtest
```

On Pi4 another SSH terminal, find its LAN IPv4 address using `hostname -I`.
On Pi3B Browser type `http://<Pi4-LAN-IP>/`. Don't use `<mDNS-hostname>` for the
first test (mDNS hostname resolution isn't guaranteed by Circle's DNS client).

## Diagnostics

At boot, before desktop, UART 115200 and HDMI show `[V03-01]` to `[V03-06]`.
`[V03-07]` is sent to UART. If the screen hangs, report the last milestone.
Network errors are sent to UART Logger if serial initialized.
Browser status reports DNS resolution / HTTP GET / error / HTTP 200.

## Attribution

Based on XD OS (`eklavya-raj/xdos`, MIT) and Circle (`rsta2/circle`, GPLv3).
This package contains separate original glue/port source and `xdos_gui.S` adapted
from source concepts. Linking or redistributing a firmware with Circle is subject
to its GPLv3 requirements. Retain upstream notices and follow licensing terms.


Replace BUILD_USER, BUILD_HOST and /path/to/xdos-build with your build host settings before running the examples.
