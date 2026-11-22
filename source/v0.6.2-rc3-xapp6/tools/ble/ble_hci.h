#ifndef XDOS_BLE_HCI_H
#define XDOS_BLE_HCI_H

#include <stddef.h>
#include <stdint.h>

namespace XdosBle {

// An H4 event is type, event code, parameter length, then parameters.
class HciEvents {
public:
    template<typename Handler>
    void Feed(uint8_t byte, Handler handle)
    {
        if (!used_ && byte != 0x04) return;
        frame_[used_++] = byte;
        if (used_ >= 3 && used_ == size_t(3 + frame_[2])) {
            handle(frame_[1], frame_ + 3, frame_[2]);
            used_ = 0;
        }
    }

    void Reset() { used_ = 0; }

private:
    uint8_t frame_[258];
    size_t used_ = 0;
};

struct Advertisement {
    uint8_t eventType, addressType;
    const uint8_t *address;
    const uint8_t *data;
    uint8_t dataSize;
    int8_t rssi;
};

// Legacy LE Advertising Report. Validate the entire event before reporting.
template<typename Handler>
bool ReadAdvertisements(const uint8_t *payload, size_t size, Handler handle)
{
    if (size < 2 || payload[0] != 0x02 || !payload[1]) return false;
    const unsigned reports = payload[1];
    size_t offset = 2;
    for (unsigned i = 0; i < reports; ++i) {
        if (size - offset < 10) return false;
        const unsigned length = payload[offset + 8];
        if (length > 31 || size - offset < 10 + length) return false;
        offset += 10 + length;
    }
    if (offset != size) return false;

    offset = 2;
    for (unsigned i = 0; i < reports; ++i) {
        const uint8_t *report = payload + offset;
        const uint8_t length = report[8];
        handle(Advertisement{report[0], report[1], report + 2, report + 9,
                             length, static_cast<int8_t>(report[9 + length])});
        offset += 10 + length;
    }
    return true;
}

}
#endif
