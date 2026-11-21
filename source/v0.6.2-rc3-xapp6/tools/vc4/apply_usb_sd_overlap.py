"""Schedule SD and icon work during the Circle USB boot settling waits."""
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


def write(path, text):
    path.write_text(text, encoding="utf-8")


header = args.sample / "kernel.h"
text = header.read_text()
write(header, replace(text, "    friend class CNetworkInitTask;",
    "    friend class CNetworkInitTask;\n    friend class CDesktopStorageBootTask;\n"
    "    void InitializeStorageAndDraw();"))

path = args.sample / "kernel.cpp"
text = path.read_text()
text = replace(text, "bool xdosBootIconsDrawn = false;", """bool xdosBootIconsDrawn = false;

namespace {
bool usbBootOverlapActive = false;
unsigned rootWaitMs = 0, hubPowerWaitMs = 0, hubResetWaitMs = 0;
volatile bool *usbBootStorageDone = nullptr;
void (*usbBootDraw)(void *) = nullptr;
void *usbBootDrawContext = nullptr;
}

extern "C" void xdos_usb_boot_wait(unsigned ms, unsigned stage)
{
    if (!usbBootOverlapActive || CurrentExecutionLevel() != TASK_LEVEL) {
        CTimer::SimpleMsDelay(ms);
        return;
    }
    if (stage == 0) rootWaitMs += ms;
    else if (stage == 1) hubPowerWaitMs += ms;
    else hubResetWaitMs += ms;
    if (stage == 0) {
        CScheduler::Get()->MsSleep(ms);
        return;
    }
    const unsigned start = CTimer::GetClockTicks();
    const unsigned waitUs = ms * 1000;
    while (true) {
        if (usbBootDraw && *usbBootStorageDone) {
            usbBootDraw(usbBootDrawContext);
            usbBootDraw = nullptr;
        }
        const unsigned elapsed = CTimer::GetClockTicks()-start;
        if (elapsed >= waitUs) break;
        const unsigned remaining = waitUs-elapsed;
        CScheduler::Get()->usSleep(usbBootDraw && remaining>10000 ? 10000 : remaining);
    }
}

class CDesktopStorageBootTask : public CTask
{
public:
    CDesktopStorageBootTask(CKernel &kernel, volatile bool &finished)
        : CTask(64*1024), kernel_(kernel), finished_(finished) {}
    void Run() override
    {
        kernel_.InitializeDesktopStorage();
        finished_ = true;
    }
private:
    CKernel &kernel_;
    volatile bool &finished_;
};""")

start = text.index("    InitializeDesktopStorage();\n#if XDOS_USE_LVGL")
end = text.index('    LogBootTiming("usb_start");', start)
storage = text[start:end]
storage = replace(storage, "    InitializeDesktopStorage();\n", "")
text = text[:start] + """    volatile bool desktopStorageReady = false;
    new CDesktopStorageBootTask(*this, desktopStorageReady);
    usbBootStorageDone = &desktopStorageReady;
    usbBootDrawContext = this;
    usbBootDraw = [](void *ctx) { static_cast<CKernel *>(ctx)->InitializeStorageAndDraw(); };
    usbBootOverlapActive = true;
""" + text[end:]
text = replace(text, "    if (!usb_.Initialize()) {\n        Debug(\"[BOOT ERROR]",
    """    const bool usbReady = usb_.Initialize();
    usbBootOverlapActive = false;
    if (usbReady) LogBootTiming("usb_ready");
    while (!desktopStorageReady) scheduler_.MsSleep(1);
    if (usbBootDraw) {
        usbBootDraw(this);
        usbBootDraw = nullptr;
    }
    usbBootStorageDone = nullptr;
    CString usbWaits;
    usbWaits.Format("[USB BOOT] root_wait_ms=%u hub_power_wait_ms=%u hub_reset_wait_ms=%u\\r\\n",
                   rootWaitMs,hubPowerWaitMs,hubResetWaitMs);
    Debug(usbWaits);
    if (!usbReady) {
        Debug("[BOOT ERROR]""")
text = replace(text, '    LogBootTiming("usb_ready");\n    AttachDevices',
               "    AttachDevices")
text = replace(text, 'void CKernel::SDKLog(void *ctx,const char *message)',
               "void CKernel::InitializeStorageAndDraw()\n{\n" + storage +
               "}\n\nvoid CKernel::SDKLog(void *ctx,const char *message)")
text = replace(text, "            kbd->RegisterKeyPressedHandler(KeyHandler);",
               '            kbd->RegisterKeyPressedHandler(KeyHandler);\n            LogBootTiming("keyboard_attached");')
write(path, text)

usb = args.circle / "lib/usb"
path = usb / "dwhcidevice.cpp"
text = path.read_text()
text = replace(text, "boolean CDWHCIDevice::EnableRootPort (void)",
              'extern "C" void xdos_usb_boot_wait(unsigned, unsigned);\n\nboolean CDWHCIDevice::EnableRootPort (void)')
start = text.index("boolean CDWHCIDevice::EnableRootPort (void)")
end = text.index("\n}", start)
root_port = text[start:end]
for ms in (100, 50, 20):
    root_port = replace(root_port, f"m_pTimer->MsDelay ({ms});", f"xdos_usb_boot_wait({ms}, 0);")
text = text[:start] + root_port + text[end:]
write(path, text)
path = usb / "usbstandardhub.cpp"
text = path.read_text()
text = replace(text, "static const char FromHub[]", 'extern "C" void xdos_usb_boot_wait(unsigned, unsigned);\n\nstatic const char FromHub[]')
text = replace(text, "CTimer::Get ()->MsDelay (nMsDelay);", "xdos_usb_boot_wait(nMsDelay, 1);")
text = replace(text, "CTimer::Get ()->MsDelay (100);", "xdos_usb_boot_wait(100, 2);")
write(path, text)
