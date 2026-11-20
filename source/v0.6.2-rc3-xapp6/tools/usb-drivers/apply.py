"""Install the optional USB module factory in a wired XDOS build."""

from pathlib import Path
import re
import shutil
import sys


source, circle = map(Path, sys.argv[1:])
tools = Path(__file__).parent


def edit(path, old, new):
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"{path.name}: missing USB module anchor {old!r}")
    path.write_text(text.replace(old,new), encoding="utf-8")


shutil.copyfile(tools/"xdos_usb_drivers.h",circle/"include/circle/usb/xdos_usb_drivers.h")
shutil.copyfile(tools/"usbdevicefactory.cpp",circle/"lib/usb/usbdevicefactory.cpp")
(source/"driver_loader.cpp").write_text(
    (tools/"driver_loader.cpp").read_text(encoding="utf-8").replace(
        '"xdos_usb_drivers.h"',"<circle/usb/xdos_usb_drivers.h>"),
    encoding="utf-8")

edit(source/"Makefile","app_shortcut.o xapp_loader.o","app_shortcut.o driver_loader.o xapp_loader.o")
edit(source/"xapp_loader.h","    static Result ExecuteFile","    static uint32_t KernelFingerprint();\n    static Result ExecuteFile")
edit(source/"xapp_loader.cpp","static uint32_t KernelFingerprint()","uint32_t XappLoaderV1::KernelFingerprint()")
edit(source/"kernel.h","CUSBKeyboardDevice *volatile keyboard_;","CDevice *volatile keyboard_;")
edit(source/"kernel.cpp",'#include "uart_service.h"',
     '#include "uart_service.h"\n#include <circle/usb/xdos_usb_drivers.h>')
edit(source/"kernel.cpp",'CUSBKeyboardDevice *kbd=(CUSBKeyboardDevice*)names_.GetDevice("ukbd1",FALSE);',
     'auto *kbd=names_.GetDevice("ukbd1",FALSE);')
edit(source/"kernel.cpp","kbd->RegisterKeyPressedHandler(KeyHandler);","XdosUsbKeyboardHandler(kbd,KeyHandler);")
edit(source/"kernel.cpp","keyboard_->UpdateLEDs();","XdosUsbKeyboardUpdateLeds(keyboard_);")

shortcut = source/"app_shortcut.cpp"
content = shortcut.read_text(encoding="utf-8")
for previous, current in (("WIDGET.XAPP", "U2WIDGET.XAPP"),
                          ("BENCH.XAPP", "U2BENCH.XAPP"),
                          ("B2WEB.XAPP", "U2BROWSE.XAPP")):
    content, count = re.subn(r'(?<=SD:/XDOS/INBOX/)[A-Z0-9]*' + re.escape(previous),
                             current, content)
    if previous != "B2WEB.XAPP" and count != 1:
        raise RuntimeError(f"Missing inbox path for {previous}")
shortcut.write_text(content, encoding="utf-8")

# USB device matching can open a module on the SD card, so mount the SD first.
kernel = source/"kernel.cpp"
text = kernel.read_text(encoding="utf-8")
start = text.index("    // FAT32 is needed")
end = text.index('    desktop_.SetNetwork(false,"ETH: DETECTING...");',start)
mount = text[start:end]
text = text[:start] + text[end:]
anchor = '    LogBootTiming("usb_start");'
if text.count(anchor) != 1:
    raise RuntimeError("USB boot anchor missing")
kernel.write_text(text.replace(anchor,mount+anchor),encoding="utf-8")

lvgl = circle/"addon/lvgl/lvgl.cpp"
edit(circle/"addon/lvgl/lvgl.h",
     "static void MouseRemovedHandler (CDevice *pDevice, void *pContext);",
     "static void MouseRemovedHandler (CDevice *pDevice, void *pContext);\n"
     "\tstatic void TouchRemovedHandler (CDevice *pDevice, void *pContext);")
edit(lvgl,"\t\t\tm_pTouchScreen->RegisterEventHandler (TouchScreenEventHandler);",
     "\t\t\tm_pTouchScreen->RegisterEventHandler (TouchScreenEventHandler);\n"
     "\t\t\tm_pTouchScreen->RegisterRemovedHandler (TouchRemovedHandler);")
edit(lvgl,"\tunsigned nTicks = CTimer::Get ()->GetClockTicks ();\n\t// Adaptive LVGL timer",
     """    if (bPlugAndPlayUpdated && !m_pTouchScreen)
    {
        m_pTouchScreen = static_cast<CTouchScreenDevice *>(
            CDeviceNameService::Get()->GetDevice("touch1",FALSE));
        if (m_pTouchScreen)
        {
            m_pTouchScreen->Setup(m_pDisplay);
            const auto *calibration = CKernelOptions::Get()->GetTouchScreen();
            if (calibration) m_pTouchScreen->SetCalibration(calibration);
            m_pTouchScreen->RegisterEventHandler(TouchScreenEventHandler);
            m_pTouchScreen->RegisterRemovedHandler(TouchRemovedHandler);
        }
    }

\tunsigned nTicks = CTimer::Get ()->GetClockTicks ();
\t// Adaptive LVGL timer""")
edit(lvgl,"else if (m_pTouchScreen != 0)","if (m_pTouchScreen != 0)")
edit(lvgl,"void CLVGL::TouchScreenEventHandler", """void CLVGL::TouchRemovedHandler(CDevice *,void *)
{
    s_pThis->m_pTouchScreen = nullptr;
    s_pThis->m_PointerData.state = LV_INDEV_STATE_RELEASED;
}

void CLVGL::TouchScreenEventHandler""")
