#include "xdos_usb_drivers.h"
#include <circle/usb/usbdevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/usb/usbkeyboard8bitdo.h>
#include <circle/usb/usbmassdevice.h>
#include <circle/usb/usbmouse.h>
#include <circle/usb/usbtouchscreen.h>

extern "C" CUSBFunction *xdos_driver_create(CUSBFunction *parent)
{
#if XDOS_DRIVER == 0
    auto *vendor = parent->GetDevice()->GetName(DeviceNameVendor);
    const bool retro = vendor && (!vendor->Compare("ven2dc8-5200") ||
                                  !vendor->Compare("ven2dc8-5201"));
    delete vendor;
    if (retro) return new CUSBKeyboard8BitDoDevice(parent);
    return new CUSBKeyboardDevice(parent);
#elif XDOS_DRIVER == 1
    return new CUSBMouseDevice(parent);
#elif XDOS_DRIVER == 2
    return new CUSBBulkOnlyMassStorageDevice(parent);
#elif XDOS_DRIVER == 3
    return new CUSBTouchScreenDevice(parent);
#else
#error Unknown USB driver
#endif
}

#if XDOS_DRIVER == 0
extern "C" void xdos_driver_keyboard_handler(CDevice *device,TKeyPressedHandler *handler)
{
    static_cast<CUSBKeyboardDevice *>(device)->RegisterKeyPressedHandler(handler);
}

extern "C" void xdos_driver_keyboard_leds(CDevice *device)
{
    static_cast<CUSBKeyboardDevice *>(device)->UpdateLEDs();
}
#endif
