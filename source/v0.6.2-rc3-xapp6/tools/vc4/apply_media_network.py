"""Use staged Ethernet assets for a MultiCore media build on an existing SD."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
kernel = root / "kernel.cpp"
text = kernel.read_text(encoding="utf-8")
anchor = 'desktop_.ScanDesktopApps();'
assert text.count(anchor) == 1
text = text.replace(anchor, """static const char *const appNames[] = {
        "BROWSER", "CALC", "FILES", "LVGLBENCH", "LVGLDEMO", "MUSIC",
        "NOTE", "PAINT", "PLAYER", "TERMINAL"
    };
    unsigned installed = 0;
    for (unsigned i=0; i<sizeof appNames/sizeof *appNames; ++i) {
        char staged[]="SD:/XDOS/INBOX/MNAPP0.APP";
        staged[sizeof staged-6]=char('0'+i);
        char destination[48]="SD:/XDOS/APPS/";
        strcat(destination,appNames[i]);
        strcat(destination,".APP");
        if (f_rename(staged,destination)==FR_OK) ++installed;
    }
    if (f_rename("SD:/XDOS/INBOX/MNTERM.XAPP","SD:/XDOS/APPS/TERMINAL.XAPP")==FR_OK)
        Debug("[MCMEDIA] Installed Terminal executable\\r\\n");
    if (installed) Debug("[MCMEDIA] Installed staged APP manifests\\r\\n");
    desktop_.ScanDesktopApps();
    Debug("[MCMEDIA] Desktop catalog scanned\\r\\n");""", 1)
text = text.replace("-MCMedia / Pi 3 ARM32", "-MCMediaNet / Pi 3 ARM32")
kernel.write_text(text, encoding="utf-8")
for name in ("desktop_apps.cpp", "app_shortcut.cpp"):
    path = root / name
    text = path.read_text(encoding="utf-8")
    for old, new in (("PLAYER.XAPP", "MNMP4.XAPP"), ("MUSIC.XAPP", "MNMP3.XAPP"),
                     ("/INBOX/MM", "/INBOX/MN")):
        text = text.replace(old, new)
    path.write_text(text, encoding="utf-8")
