#include "ble_hcd.h"
#include <cassert>

int main()
{
    const uint8_t data[] = {0x4c, 0xfc, 2, 0x34, 0x12, 0x4e, 0xfc, 0};
    XdosBle::HcdRecord record{};
    size_t offset = 0;
    using XdosBle::HcdResult;
    using XdosBle::NextHcdRecord;
    assert(NextHcdRecord(data, sizeof data, offset, record) == HcdResult::Record);
    assert(record.opcode == 0xfc4c && record.size == 2 && record.parameters[1] == 0x12);
    assert(NextHcdRecord(data, sizeof data, offset, record) == HcdResult::Record);
    assert(record.opcode == 0xfc4e && record.size == 0);
    assert(NextHcdRecord(data, sizeof data, offset, record) == HcdResult::End);
    offset = 0;
    assert(NextHcdRecord(data, 2, offset, record) == HcdResult::Truncated && offset == 0);
    assert(NextHcdRecord(data, 4, offset, record) == HcdResult::Truncated && offset == 0);
}
