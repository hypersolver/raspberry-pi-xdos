#include "ble_hci.h"
#include <cassert>

int main()
{
    XdosBle::HciEvents events;
    const uint8_t frame[] = {
        0x04, 0x3e, 15, 0x02, 1, 0, 1, 1, 2, 3, 4, 5, 6, 3, 2, 1, 6, 0xc4
    };
    unsigned count = 0;
    for (const auto byte : frame) {
        events.Feed(byte, [&](uint8_t event, const uint8_t *payload, size_t size) {
            assert(event == 0x3e);
            auto handle = [&](const XdosBle::Advertisement &report) {
                assert(report.addressType == 1 && report.address[0] == 1);
                assert(report.dataSize == 3 && report.rssi == -60);
                ++count;
            };
            assert(XdosBle::ReadAdvertisements(payload, size, handle));
            assert(!XdosBle::ReadAdvertisements(payload, size - 1, handle));
        });
    }
    assert(count == 1);
}
