#include "ble_h4_link.h"
#include <cassert>
#include <vector>

struct Serial {
    std::vector<int> input;
    std::vector<uint8_t> output;
    unsigned pos = 0;
    bool blocked = false;
    int Read() { return pos < input.size() ? input[pos++] : -1; }
    bool Write(uint8_t n)
    {
        if (blocked) return false;
        output.push_back(n);
        return true;
    }
};

int main()
{
    Serial serial;
    uint32_t clock = 0;
    XdosBle::H4Link link(serial, [&] { return clock++; }, [] {});
    serial.input = {4, 14, 4, 1, 1, 0x0c, 0, 4, 14, 4, 1, 3, 0x0c, 0};
    assert(link.Command(0x0c03, nullptr, 0, 40));
    assert((serial.output == std::vector<uint8_t>{1, 3, 0x0c, 0}));
    serial.input.insert(serial.input.end(), {4,15,4,1,1,3,0x0c});
    assert(!link.Command(0x0c03, nullptr, 0, 40));
    assert(!link.Command(0x0c03, nullptr, 0, 10));
    serial.blocked = true;
    clock = UINT32_MAX - 2;
    assert(!link.Command(0x0c03, nullptr, 0, 10));
}
