#include "ble_h4_link.h"
#include "ble_mini_uart.h"
#include "ble_power.h"
#include "ble_scan.h"
#include "xdos_corejobs.h"
#include "xdos_sdk_v1.h"
#include "xdos_xapp_memory.h"

#include <circle/machineinfo.h>
#include <circle/new.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <fatfs/ff.h>
#include <lvgl.h>

extern "C" int32_t xapp_entry(const XdosHostApiV1 *api);

namespace {
using namespace XdosBle;
unsigned Tick() { return CTimer::GetClockTicks(); }
void Yield() { CScheduler::Get()->Yield(); }
using Link = H4Link<MiniUart, decltype(&Tick), decltype(&Yield)>;

struct Scanner {
    Scanner() : link(uart, Tick, Yield) {}
    MiniUart uart;
    Link link;
    const XdosHostApiV1 *api;
    lv_timer_t *timer;
    int slot;
    bool powered;
    unsigned reports;
};
alignas(Scanner) uint8_t scannerStorage[sizeof(Scanner)];

void Delay(unsigned ms)
{
    const unsigned start = Tick();
    while (Tick() - start < ms * 1000) Yield();
}

void Log(const Scanner &scanner, const char *text)
{
    scanner.api->log(scanner.api->context, text);
}

void Stop(Scanner &scanner)
{
    if (scanner.timer) lv_timer_delete(scanner.timer);
    if (scanner.powered) SetControllerPower(false);
    scanner.uart.Stop();
    scanner.~Scanner();
}

void Close(void *context)
{
    auto &scanner = *static_cast<Scanner *>(context);
    const int slot = scanner.slot;
    Stop(scanner);
    xdos_v2_exit(slot);
}

void Poll(lv_timer_t *timer)
{
    auto &scanner = *static_cast<Scanner *>(lv_timer_get_user_data(timer));
    scanner.link.Poll(192, [&](const Advertisement &) {
        if (++scanner.reports <= 8)
            Log(scanner, "[BLE] Advertising report received\r\n");
    });
}

bool LoadFirmwareAndScan(Scanner &scanner)
{
    FIL file{};
    if (f_open(&file, "SD:/XDOS/INBOX/BCM4345.HCD", FA_READ) != FR_OK) {
        Log(scanner, "[BLE] Missing BCM4345.HCD\r\n");
        return false;
    }
    const size_t length = f_size(&file);
    auto *firmware = length && length <= 64 * 1024
        ? static_cast<uint8_t *>(xdos_v2_alloc(scanner.slot, length)) : nullptr;
    UINT read = 0;
    const bool loaded = firmware && f_read(&file, firmware, length, &read) == FR_OK
                        && read == length;
    f_close(&file);
    if (!loaded) { Log(scanner, "[BLE] HCD read or quota failed\r\n"); return false; }

    const unsigned clock = CMachineInfo::Get()->GetClockRate(CLOCK_ID_CORE);
    if (!scanner.uart.Start(clock)) { Log(scanner, "[BLE] AUX UART unavailable\r\n"); return false; }
    if (!SetControllerPower(false)) { Log(scanner, "[BLE] BT_ON low failed\r\n"); return false; }
    Delay(30);
    if (!SetControllerPower(true)) { Log(scanner, "[BLE] BT_ON high failed\r\n"); return false; }
    scanner.powered = true;
    Delay(160);
    const auto result = StartPassiveScan(firmware, length,
        [&](uint16_t opcode, const uint8_t *data, size_t size) {
            return scanner.link.Command(opcode, data, size, 200000);
        }, Delay);
    xdos_v2_free(scanner.slot, firmware);
    if (result == ScanStart::ResetFailed)
        Log(scanner, "[BLE] HCI Reset timed out or was rejected\r\n");
    if (result == ScanStart::DownloadFailed)
        Log(scanner, "[BLE] Firmware command timed out or was rejected\r\n");
    if (result == ScanStart::InvalidFirmware)
        Log(scanner, "[BLE] Invalid HCD firmware format\r\n");
    if (result == ScanStart::SetupFailed)
        Log(scanner, "[BLE] Scan setup timed out or was rejected\r\n");
    return result == ScanStart::Ready;
}
}

extern "C" int32_t xapp_entry(const XdosHostApiV1 *api)
{
    const int slot = xdos_v2_slot_of(reinterpret_cast<const void *>(xapp_entry));
    if (!api || !api->log || slot < 0) return -1;
    auto &scanner = *new (scannerStorage) Scanner();
    scanner.api = api;
    scanner.slot = slot;
    scanner.timer = nullptr;
    scanner.powered = false;
    scanner.reports = 0;

    if (LoadFirmwareAndScan(scanner))
        scanner.timer = lv_timer_create(Poll, 10, &scanner);
    if (!scanner.timer) {
        Stop(scanner);
        return -1;
    }
    xdos_v2_register_close(slot, Close, &scanner);
    Log(scanner, "[BLE] Passive scan started\r\n");
    return 0;
}
