#ifndef XDOS_BLE_SCAN_H
#define XDOS_BLE_SCAN_H

#include "ble_hcd.h"

namespace XdosBle {
enum class ScanStart { Ready, InvalidFirmware, ResetFailed, DownloadFailed, SetupFailed };

// command() sends one H4 command and waits for a matching successful reply
// with a bounded timeout. settle() waits for the controller to restart.
template<typename Command, typename Settle>
ScanStart StartPassiveScan(const uint8_t *firmware, size_t size,
                           Command command, Settle settle)
{
    constexpr uint16_t reset = 0x0c03;
    constexpr uint16_t downloadMiniDriver = 0xfc2e;
    constexpr uint16_t launchRam = 0xfc4e;
    constexpr uint16_t eventMask = 0x0c01;
    constexpr uint16_t leEventMask = 0x2001;
    constexpr uint16_t scanParameters = 0x200b;
    constexpr uint16_t scanEnable = 0x200c;
    HcdRecord record{};
    size_t offset = 0;
    if (!firmware || !size) return ScanStart::InvalidFirmware;
    bool launched = false;
    while (offset < size) {
        if (NextHcdRecord(firmware, size, offset, record) != HcdResult::Record || launched)
            return ScanStart::InvalidFirmware;
        launched = record.opcode == launchRam;
    }
    if (!launched) return ScanStart::InvalidFirmware;
    if (!command(reset, nullptr, 0)) return ScanStart::ResetFailed;
    if (!command(downloadMiniDriver, nullptr, 0)) return ScanStart::DownloadFailed;
    settle(50);
    offset = 0;
    while (NextHcdRecord(firmware, size, offset, record) == HcdResult::Record)
        if (!command(record.opcode, record.parameters, record.size)) return ScanStart::DownloadFailed;
    settle(250);
    if (!command(reset, nullptr, 0)) return ScanStart::ResetFailed;

    const uint8_t events[] = {0, 0, 0, 0, 0, 0, 0, 0x20};
    const uint8_t leEvents[] = {2, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t parameters[] = {0, 0x30, 0, 0x30, 0, 0, 0};
    const uint8_t enable[] = {1, 1};
    if (!command(eventMask, events, sizeof events)
        || !command(leEventMask, leEvents, sizeof leEvents)
        || !command(scanParameters, parameters, sizeof parameters)
        || !command(scanEnable, enable, sizeof enable))
        return ScanStart::SetupFailed;
    return ScanStart::Ready;
}
}
#endif
