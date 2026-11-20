#pragma once

#include <circle/types.h>
#include <circle/input/keyboardbehaviour.h>

class CUSBFunction;
class CDevice;

enum class XdosUsbDriver : u32 { Keyboard, Mouse, Storage, Touch };

// The application owns the first 320 KiB of the executable section.
// Driver slots start immediately after it and remain resident until reboot.
constexpr u32 XdosDriverBase = 0x00350000;
constexpr u32 XdosDriverSlotBytes = 0x00020000;
constexpr u32 XdosDriverMagic = 0x31524458; // XDR1
constexpr u32 XdosDriverCount = 4;

struct XdosUsbDriverHeader {
    u32 magic, driver, imageBytes, kernelCrc, loadAddress;
    u32 createOffset, keyHandlerOffset, updateLedsOffset;
    u32 initStartOffset, initEndOffset;
};

CUSBFunction *XdosLoadUsbFunction(XdosUsbDriver driver,CUSBFunction *parent);
void XdosUsbKeyboardHandler(CDevice *device,TKeyPressedHandler *handler);
void XdosUsbKeyboardUpdateLeds(CDevice *device);
