# XDOS XAPP Desktop Shortcut and Icon Specification

[繁體中文](xapp-desktop-spec.zh-TW.md) · [Arduino XAPP development and deployment (繁體中文)](arduino-xapp.zh-TW.md)

This specification applies to APPSCAN builds with dynamic shortcuts enabled. The older PXLAYOUT release uses fixed shortcuts.

## Discovery and capacity

1. After mounting the FAT32 boot partition, scan the top level of `SD:/XDOS/APPS/` for `.APP` files. The extension is case-insensitive.
2. Accept the first **10** valid manifests in FAT directory order. Skip subdirectories, invalid manifests and entries with missing executables. Stop scanning immediately after the tenth accepted entry.
3. Reboot to rescan after changing the card. The loader checks the actual executable ABI and applicable kernel fingerprint at launch.

## Manifest format

Save the following as `ARDDEMO.APP` and place the matching executable at the declared path.

```ini
[Application]
Name=Arduino Demo
Exec=SD:/XDOS/APPS/ARDDEMO.XAPP
ABI=1
Arch=armv7-a32
Type=Arduino
```

| Field | Rule |
| --- | --- |
| `[Application]` | Section and key names are case-sensitive. Manifests are 1–1024 bytes of plain text with LF or CRLF. ASCII without a BOM is recommended. |
| `Name=` | Required and nonempty. Supplies the desktop label and icon letter. |
| `Exec=` | Required. Points to an existing `.XAPP` in the top level of `SD:/XDOS/APPS/` or `SD:/XDOS/INBOX/`. The file stem may contain ASCII letters, digits, underscores and hyphens. |
| `ABI=` and `Arch=` | Currently `1` and `armv7-a32`, respectively. |
| `Type=Arduino` | Selects the sketch lifecycle API. The executable must match the kernel fingerprint. |
| `Icon=` | Dynamic shortcuts derive the icon from `Name=`. Legacy manifests may retain this field. |

## Layout, labels and interaction

- Use two evenly distributed rows of five equally spaced slots. Fill the top row left to right, then the bottom row. Leave unused slots empty.
- Center the icon and label on the same horizontal axis, with the label beneath the icon. Labels with **1–16 display characters** can appear, including spaces. Fall back to the regular font if the enlarged font does not fit. Hide the entire label if the regular font cannot fit or the name has **17 or more** display characters.
- The current pixel font supports printable ASCII. Other bytes display as `?`, so the limit for Unicode names applies to the resulting display text.
- Clicking either the icon or the label opens the application. Icons remain clickable when labels are hidden.

## Icon initials and colors

Use the first ASCII letter in `Name=` and draw it in white uppercase. Skip leading spaces, digits and punctuation. If no ASCII letter is present, use `?` on a gray background. Each letter has a fixed background color. Common initials use more widely separated hues.

| Initial | Background (RGB) | Initial | Background (RGB) | Initial | Background (RGB) |
| --- | --- | --- | --- | --- | --- |
| A | `#007361` | J | `#4D4789` | S | `#225AA3` |
| B | `#676125` | K | `#AE415A` | T | `#7B4E1E` |
| C | `#1F6490` | L | `#5E6725` | U | `#87401F` |
| D | `#5E3979` | M | `#92442C` | V | `#126D74` |
| E | `#A7303C` | N | `#475F1F` | W | `#963F71` |
| F | `#873851` | O | `#965324` | X | `#33608E` |
| G | `#63692C` | P | `#6B4B9A` | Y | `#006758` |
| H | `#226680` | Q | `#6F5024` | Z | `#70376D` |
| I | `#6F3D9E` | R | `#9D365B` | `?` | `#4B5566` |

## Hardware verification

MultiCore APPSCAN QA3 was tested on Pi 3B+ at 1920×1080. UART reported 11 manifests and 10 loaded shortcuts. HDMI verification confirmed two rows of five, a centered `CENTERED LABEL16`, omission of the eleventh entry, and successful launch by clicking an icon whose 17-character label was hidden.
