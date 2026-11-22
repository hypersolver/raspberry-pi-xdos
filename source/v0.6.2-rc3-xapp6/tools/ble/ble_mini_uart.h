#ifndef XDOS_BLE_MINI_UART_H
#define XDOS_BLE_MINI_UART_H

#include <circle/bcm2835.h>
#include <circle/gpiopin.h>
#include <circle/memio.h>
#include <stdint.h>

namespace XdosBle {
// Pi 3B+ onboard Bluetooth UART. PL011 and GPIO14/15 are untouched.
// The caller must keep the supplied VPU core clock stable.
class MiniUart {
public:
    bool Start(unsigned coreClockHz)
    {
        constexpr unsigned baud = 115200;
        constexpr unsigned inputDivider = 8 * baud;
        const unsigned divisor = (coreClockHz + inputDivider / 2) / inputDivider;
        if (!divisor || divisor > 65536 || active_ ||
            PinFunction(14) == Alt5 || PinFunction(15) == Alt5)
            return false;

        wasEnabled_ = (read32(ARM_AUX_ENABLE) & 1) != 0;
        const bool btPinsInUse = PinFunction(32) == Alt5 && PinFunction(33) == Alt5;
        if (wasEnabled_ && ((read32(InterruptEnable) & 0x0f) ||
                           (btPinsInUse && read32(Control) == ScanControl)))
            return false;
        if (wasEnabled_) {
            oldControl_ = read32(Control);
            oldLine_ = read32(LineControl);
            oldModem_ = read32(ModemControl);
            oldBaud_ = read32(Baud);
        }
        for (unsigned i = 0; i < 4; ++i)
            previousFunctions_[i] = PinFunction(30 + i);

        cts_.AssignPin(30);
        rts_.AssignPin(31);
        tx_.AssignPin(32);
        rx_.AssignPin(33);
        CGPIOPin *const pins[] = {&cts_, &rts_, &tx_, &rx_};
        for (auto *pin : pins)
            pin->SetMode(GPIOModeAlternateFunction5);
        cts_.SetPullMode(GPIOPullModeUp);
        rx_.SetPullMode(GPIOPullModeUp);

        write32(ARM_AUX_ENABLE, read32(ARM_AUX_ENABLE) | 1);
        write32(Control, 0);
        write32(InterruptEnable, 0);
        write32(LineControl, 3);
        write32(ModemControl, 0);
        write32(InterruptIdentify, 0xc6);
        write32(Baud, divisor - 1);
        write32(Control, ScanControl);
        active_ = true;
        return true;
    }

    // Stop the Bluetooth transport before releasing its owning module.
    void Stop()
    {
        if (!active_) return;
        write32(Control, 0);
        for (unsigned i = 0; i < 4; ++i)
            SetPinFunction(30 + i, previousFunctions_[i]);
        if (wasEnabled_) {
            write32(LineControl, oldLine_);
            write32(ModemControl, oldModem_);
            write32(Baud, oldBaud_);
            write32(Control, oldControl_);
        } else {
            write32(ARM_AUX_ENABLE, read32(ARM_AUX_ENABLE) & ~1u);
        }
        active_ = false;
    }

    int Read() const
    {
        if (!active_ || !(read32(LineStatus) & 1)) return -1;
        return int(read32(Data) & 0xff);
    }

    bool Write(uint8_t value) const
    {
        if (!active_ || !(read32(LineStatus) & (1u << 5))) return false;
        write32(Data, value);
        return true;
    }

private:
    static constexpr unsigned Alt5 = 2;
    // Broadcom HCI UART uses active-low CTS and RTS. AUX defaults to
    // active-high unless both polarity bits are explicitly set.
    static constexpr unsigned ScanControl = 0xcf;

    static unsigned PinFunction(unsigned pin)
    {
        return (read32(ARM_GPIO_GPFSEL0 + 4 * (pin / 10)) >> (3 * (pin % 10))) & 7;
    }

    static void SetPinFunction(unsigned pin, unsigned function)
    {
        const uintptr_t address = ARM_GPIO_GPFSEL0 + 4 * (pin / 10);
        const unsigned shift = 3 * (pin % 10);
        write32(address, (read32(address) & ~(7u << shift)) | (function << shift));
    }

    enum : uintptr_t {
        Data = ARM_AUX_BASE + 0x40,
        InterruptEnable = ARM_AUX_BASE + 0x44,
        InterruptIdentify = ARM_AUX_BASE + 0x48,
        LineControl = ARM_AUX_BASE + 0x4c,
        ModemControl = ARM_AUX_BASE + 0x50,
        LineStatus = ARM_AUX_BASE + 0x54,
        Control = ARM_AUX_BASE + 0x60,
        Baud = ARM_AUX_BASE + 0x68
    };
    bool active_ = false;
    bool wasEnabled_ = false;
    unsigned oldControl_, oldLine_, oldModem_, oldBaud_;
    unsigned previousFunctions_[4];
    CGPIOPin cts_, rts_, tx_, rx_;
};
}
#endif
