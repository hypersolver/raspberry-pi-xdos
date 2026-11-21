"""Shorten SD readiness polling and draw desktop icons before USB enumeration."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("circle", type=Path)
parser.add_argument("sample", type=Path)
args = parser.parse_args()

def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit(f"Unexpected source landmark: {old}")
    return text.replace(old, new, 1)

emmc_path = args.circle / "addon/SDCard/emmc.cpp"
emmc = emmc_path.read_text()
emmc = replace(emmc, "int card_is_busy = 1;\n\twhile",
    "int card_is_busy = 1;\n\tunsigned sdPolls = 0;\n\tconst unsigned sdPollStart = CTimer::GetClockTicks ();\n\twhile")
emmc = replace(emmc, "\t\tif ((m_last_r0 >> 31) & 1)\n", "\t\t++sdPolls;\n\t\tif ((m_last_r0 >> 31) & 1)\n")
emmc = replace(emmc, "\t\t\tcard_is_busy = 0;",
    '\t\t\tLogWrite (LogNotice, "[SD READY] polls=%u elapsed_us=%u", sdPolls, CTimer::GetClockTicks () - sdPollStart);\n\t\t\tcard_is_busy = 0;')
emmc = replace(emmc, "\t\t\tusDelay (500000);",
    "\t\t\t// Keep MMC's existing cadence. SD reports readiness via ACMD41.\n\t\t\tusDelay (m_Device == EmbeddedMMC ? 500000 : 20000);")
emmc_path.write_text(emmc)

header_path = args.sample / "kernel.h"
header = header_path.read_text()
header = replace(header, "    void InitializeNetworkAfterDesktop();",
                 "    void InitializeDesktopStorage();\n    void InitializeNetworkAfterDesktop();")
header_path.write_text(header)

path = args.sample / "kernel.cpp"
kernel = path.read_text()
start = kernel.index("    if(!storageMounted_) {\n        LogBootTiming(\"icons_sd_start\");")
end = kernel.index('    scheduler_.MsSleep(350); // keep PHY probing deferred', start)
storage = kernel[start:end]
kernel = kernel[:start] + kernel[end:]
kernel = replace(kernel, "void CKernel::InitializeNetworkAfterDesktop()",
    "void CKernel::InitializeDesktopStorage()\n{\n" + storage + "}\n\nvoid CKernel::InitializeNetworkAfterDesktop()")
kernel = replace(kernel, '    LogBootTiming("usb_start");', '''    InitializeDesktopStorage();
#if XDOS_USE_LVGL
    if (storageMounted_) {
        ui_.Refresh(CTimer::GetClockTicks());
        lv_refr_now(lv_display_get_default());
        const unsigned iconsFlushStart=CTimer::GetClockTicks();
        while (xdos_p0_dma_submitted != xdos_p0_dma_completed &&
               (unsigned)(CTimer::GetClockTicks()-iconsFlushStart) < 100000U)
            scheduler_.MsSleep(1);
        xdos_present_frame();
        if (xdosBootIconsDrawn) LogBootTiming("icons_first_draw");
    }
#endif
    LogBootTiming("usb_start");''')
kernel = replace(kernel,"    bool iconsReported = false;", "    bool iconsReported = xdosBootIconsDrawn;")
path.write_text(kernel)
