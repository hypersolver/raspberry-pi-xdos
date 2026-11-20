#include <circle/usb/usbdevicefactory.h>
#include <circle/usb/usbhid.h>
#include <circle/usb/usbstandardhub.h>
#include <circle/usb/lan7800.h>
#include <circle/usb/xdos_usb_drivers.h>
#include <circle/koptions.h>
#include <circle/synchronize.h>

CUSBFunction *CUSBDeviceFactory::GetDevice(CUSBFunction *parent,CString *name)
{
    CUSBFunction *device = nullptr;
    if (name->Compare(CKernelOptions::Get()->GetUSBIgnore()) != 0) {
        if (!name->Compare("int9-0-0") || !name->Compare("int9-0-2"))
            device = new CUSBStandardHub(parent);
        else if (!name->Compare("ven424-7800"))
            device = new CLAN7800Device(parent);
        else if (!name->Compare("int8-6-50"))
            device = XdosLoadUsbFunction(XdosUsbDriver::Storage,parent);
        else if (!name->Compare("int3-1-1"))
            device = XdosLoadUsbFunction(XdosUsbDriver::Keyboard,parent);
        else if (!name->Compare("int3-1-2"))
            device = XdosLoadUsbFunction(XdosUsbDriver::Mouse,parent);
        else if (!name->Compare("int3-0-0") || !name->Compare("int3-0-2") ||
                 !name->Compare("int3-1-0"))
            device = GetGenericHIDDevice(parent);
    }
    delete name;
    return device;
}

CUSBFunction *CUSBDeviceFactory::GetGenericHIDDevice(CUSBFunction *parent)
{
    CUSBFunction function(parent);
    const auto *hid = reinterpret_cast<const TUSBHIDDescriptor *>(
        function.GetDescriptor(DESCRIPTOR_HID));
    if (!hid || !hid->wReportDescriptorLength || hid->wReportDescriptorLength > 4096)
        return nullptr;
    const unsigned size = hid->wReportDescriptorLength;
    DMA_BUFFER(u8, Report, size);
    if (function.GetHost()->GetDescriptor(function.GetEndpoint0(),
           hid->bReportDescriptorType, DESCRIPTOR_INDEX_DEFAULT, Report, size,
           REQUEST_IN | REQUEST_TO_INTERFACE, function.GetInterfaceNumber()) != static_cast<int>(size))
        return nullptr;

    unsigned page = 0;
    for (unsigned offset = 0; offset < size;) {
        const u8 item = Report[offset++];
        if (item == 0xFE) {
            if (size-offset < 2 || Report[offset] > size-offset-2) return nullptr;
            offset += Report[offset] + 2;
            continue;
        }
        unsigned bytes = item & 3;
        if (bytes == 3) bytes = 4;
        if (bytes > size-offset) return nullptr;
        unsigned value = 0;
        for (unsigned i = 0; i < bytes; ++i) value |= unsigned(Report[offset++]) << (8*i);
        if ((item & 0xFC) == 0x04) page = value;
        if ((item & 0xFC) != 0x08) continue;
        if (page == 0x0D)
            return XdosLoadUsbFunction(XdosUsbDriver::Touch,parent);
        if (page == 1 && value == 2)
            return XdosLoadUsbFunction(XdosUsbDriver::Mouse,parent);
        if (page == 1 && value == 6)
            return XdosLoadUsbFunction(XdosUsbDriver::Keyboard,parent);
    }
    return nullptr;
}
