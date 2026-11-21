"""Profile the APPSCAN first icon draw and optionally remove its network delay."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("sample", type=Path)
parser.add_argument("--fast", action="store_true")
parser.add_argument("--profile", action="store_true")
args = parser.parse_args()
if not (args.fast or args.profile):
    parser.error("select --fast, --profile, or both")
root = args.sample

def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit(f"Unexpected source landmark: {old}")
    return text.replace(old, new, 1)

kernel_path = root / "kernel.cpp"
kernel = kernel_path.read_text()
if args.profile:
    kernel = replace(kernel, "void CKernel::LogBootTiming(const char *stage)\n{\n#if XDOS_TEST_DEBUG",
                     "void CKernel::LogBootTiming(const char *stage)\n{\n#if 1")
    kernel = replace(kernel, "    if(!storageMounted_) {\n        if(emmc_.Initialize()) {",
                     '    if(!storageMounted_) {\n        LogBootTiming("icons_sd_start");\n        if(emmc_.Initialize()) {\n            LogBootTiming("icons_sd_ready");')
    kernel = replace(kernel, '            const FRESULT fr=f_mount(&filesystem_,"SD:",1);',
                     '            const FRESULT fr=f_mount(&filesystem_,"SD:",1);\n            LogBootTiming("icons_mount_end");')
    kernel = replace(kernel, "desktop_.ScanDesktopApps();",
                     'LogBootTiming("icons_scan_start");\n    desktop_.ScanDesktopApps();\n    LogBootTiming("icons_scan_end");')
    kernel = replace(kernel, "class CNetworkInitTask : public CTask",
                     "bool xdosBootIconsDrawn = false;\nclass CNetworkInitTask : public CTask")
    kernel = replace(kernel, "    unsigned lastLedPollUs = 0;",
                     "    unsigned lastLedPollUs = 0;\n    bool iconsReported = false;")
    kernel = replace(kernel, "        lvgl_.Update(plugUpdated);",
                     '        lvgl_.Update(plugUpdated);\n        if (!iconsReported && xdosBootIconsDrawn) {\n            LogBootTiming("icons_first_draw");\n            iconsReported = true;\n        }')
if args.fast:
    kernel = replace(kernel, "        CScheduler::Get()->MsSleep(350); // let desktop/input loop run first\n", "")
    kernel = replace(kernel, '    desktop_.SetNetwork(false,"ETH: DETECTING...");',
                     '    scheduler_.MsSleep(350); // keep PHY probing deferred after drawing the shortcuts\n    desktop_.SetNetwork(false,"ETH: DETECTING...");')
kernel_path.write_text(kernel)
if args.profile:
    desktop_path = root / "desktop.cpp"
    desktop = desktop_path.read_text()
    desktop = replace(desktop,"void Desktop::DrawIcons()\n{",
                      "extern bool xdosBootIconsDrawn;\nvoid Desktop::DrawIcons()\n{\n    if (discoveredCount_) xdosBootIconsDrawn = true;")
    desktop_path.write_text(desktop)
