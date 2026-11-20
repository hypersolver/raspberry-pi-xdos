#include "xdos_usb_drivers.h"
#include "xapp_loader.h"
#include <circle/logger.h>
#include <circle/synchronize.h>
#include <fatfs/ff.h>
#include <string.h>

namespace {
constexpr const char *paths[] = {
    "SD:/XDOS/DRIVERS/KEYBOARD.DRV", "SD:/XDOS/DRIVERS/MOUSE.DRV",
    "SD:/XDOS/DRIVERS/STORAGE.DRV", "SD:/XDOS/DRIVERS/TOUCH.DRV",
};
static_assert(sizeof paths / sizeof paths[0] == XdosDriverCount, "Driver table mismatch");
constexpr const char *networkPaths[] = {
    "SD:/XDOS/INBOX/U2KEY.DRV", "SD:/XDOS/INBOX/U2MOUSE.DRV",
    "SD:/XDOS/INBOX/U2STOR.DRV", "SD:/XDOS/INBOX/U2TOUCH.DRV",
};
static_assert(sizeof networkPaths / sizeof networkPaths[0] == XdosDriverCount,
              "Network driver table mismatch");
bool loaded[XdosDriverCount]{};
bool loading[XdosDriverCount]{};

bool EntryInImage(u32 offset,u32 length)
{
    return offset >= sizeof(XdosUsbDriverHeader) && offset < length && !(offset & 3);
}

const XdosUsbDriverHeader *Load(XdosUsbDriver driver)
{
    const auto index = static_cast<u32>(driver);
    if (index >= XdosDriverCount || loading[index]) return nullptr;
    const u32 address = XdosDriverBase + index * XdosDriverSlotBytes;
    auto *slot = reinterpret_cast<XdosUsbDriverHeader *>(address);
    if (loaded[index]) return slot;

    loading[index] = true;
    FIL file;
    XdosUsbDriverHeader header{};
    UINT bytes = 0;
    bool valid = false;
    const char *path = networkPaths[index];
    auto result = f_open(&file,path,FA_READ);
    if (result != FR_OK) {
        path = paths[index];
        result = f_open(&file,path,FA_READ);
    }
    if (result == FR_OK) {
        valid = f_read(&file,&header,sizeof header,&bytes) == FR_OK &&
                bytes == sizeof header && header.magic == XdosDriverMagic &&
                header.driver == index && header.imageBytes == f_size(&file) &&
                header.imageBytes <= XdosDriverSlotBytes && header.loadAddress == address &&
                header.kernelCrc == XappLoaderV1::KernelFingerprint() &&
                EntryInImage(header.createOffset,header.imageBytes) &&
                header.initStartOffset >= sizeof header &&
                header.initStartOffset <= header.initEndOffset &&
                header.initEndOffset <= header.imageBytes &&
                !(header.initStartOffset & 3) && !(header.initEndOffset & 3) &&
                (driver != XdosUsbDriver::Keyboard ||
                 (EntryInImage(header.keyHandlerOffset,header.imageBytes) &&
                  EntryInImage(header.updateLedsOffset,header.imageBytes)));
        if (valid) {
            memset(slot,0,XdosDriverSlotBytes);
            *slot = header;
            const UINT remainder = header.imageBytes - sizeof header;
            valid = f_read(&file,slot+1,remainder,&bytes) == FR_OK && bytes == remainder;
        }
        valid = f_close(&file) == FR_OK && valid;
    }
    const auto *first = reinterpret_cast<const u32 *>(address+header.initStartOffset);
    const auto *last = reinterpret_cast<const u32 *>(address+header.initEndOffset);
    if (valid)
        for (auto *entry = first; entry != last; ++entry)
            if (*entry < address || !EntryInImage(*entry-address,header.imageBytes)) valid = false;
    if (valid) {
        SyncDataAndInstructionCache();
        for (; first != last; ++first) reinterpret_cast<void (*)()>(*first)();
        loaded[index] = true;
        CLogger::Get()->Write("USB DRV",LogNotice,"Loaded %s",path);
    } else {
        CLogger::Get()->Write("USB DRV",LogWarning,"Cannot load %s",path);
    }
    loading[index] = false;
    return valid ? slot : nullptr;
}
}

CUSBFunction *XdosLoadUsbFunction(XdosUsbDriver driver,CUSBFunction *parent)
{
    const auto *module = Load(driver);
    if (!module) return nullptr;
    auto create = reinterpret_cast<CUSBFunction *(*)(CUSBFunction *)>(
        module->loadAddress + module->createOffset);
    return create(parent);
}

void XdosUsbKeyboardHandler(CDevice *device,TKeyPressedHandler *handler)
{
    const auto *module = Load(XdosUsbDriver::Keyboard);
    if (!module) return;
    auto bind = reinterpret_cast<void (*)(CDevice *,TKeyPressedHandler *)>(
        module->loadAddress + module->keyHandlerOffset);
    bind(device,handler);
}

void XdosUsbKeyboardUpdateLeds(CDevice *device)
{
    const auto *module = Load(XdosUsbDriver::Keyboard);
    if (!module) return;
    auto update = reinterpret_cast<void (*)(CDevice *)>(
        module->loadAddress + module->updateLedsOffset);
    update(device);
}
