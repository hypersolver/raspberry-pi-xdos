#ifndef XDOS_BLE_H4_LINK_H
#define XDOS_BLE_H4_LINK_H

#include "ble_hci.h"

namespace XdosBle {
template<typename Transport, typename Tick, typename Yield>
class H4Link {
public:
    H4Link(Transport &uart, Tick tick, Yield yield)
        : uart_(uart), tick_(tick), yield_(yield) {}

    bool Command(uint16_t opcode, const uint8_t *data, size_t size, uint32_t timeout)
    {
        if (size > 255 || (size && !data)) return false;
        const uint32_t start = tick_();
        const uint8_t header[] = {1, uint8_t(opcode), uint8_t(opcode >> 8), uint8_t(size)};
        for (const auto byte : header)
            if (!WriteUntil(byte, start, timeout)) return false;
        for (size_t i = 0; i < size; ++i)
            if (!WriteUntil(data[i], start, timeout)) return false;

        int status = -1;
        while (uint32_t(tick_() - start) < timeout) {
            const int byte = uart_.Read();
            if (byte < 0) {
                yield_();
                continue;
            }
            events_.Feed(uint8_t(byte), [&](uint8_t event, const uint8_t *p, size_t count) {
                if (event == 0x0e && count >= 4 && Match(p + 1, opcode))
                    status = p[3];
                // Successful Command Status is intermediate for asynchronous commands.
                if (event == 0x0f && count >= 4 && p[0] && Match(p + 2, opcode))
                    status = p[0];
            });
            if (status >= 0) return status == 0;
        }
        events_.Reset();
        return false;
    }

    template<typename Handler>
    void Poll(unsigned budget, Handler handler)
    {
        while (budget--) {
            const int byte = uart_.Read();
            if (byte < 0) break;
            events_.Feed(uint8_t(byte), [&](uint8_t event, const uint8_t *p, size_t size) {
                if (event == 0x3e) ReadAdvertisements(p, size, handler);
            });
        }
    }

private:
    static bool Match(const uint8_t *p, uint16_t opcode)
    {
        return p[0] == uint8_t(opcode) && p[1] == uint8_t(opcode >> 8);
    }

    bool WriteUntil(uint8_t byte, uint32_t start, uint32_t timeout)
    {
        while (uint32_t(tick_() - start) < timeout) {
            if (uart_.Write(byte)) return true;
            yield_();
        }
        events_.Reset();
        return false;
    }

    Transport &uart_;
    Tick tick_;
    Yield yield_;
    HciEvents events_;
};
}
#endif
