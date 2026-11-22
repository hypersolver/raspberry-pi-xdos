#ifndef XDOS_BLE_HCD_H
#define XDOS_BLE_HCD_H

#include <stddef.h>
#include <stdint.h>

namespace XdosBle {
struct HcdRecord {
    uint16_t opcode;
    uint8_t size;
    const uint8_t *parameters;
};

enum class HcdResult { Record, End, Truncated };

// A Broadcom HCD file stores little-endian opcode, length, then parameters.
// The H4 command packet type is added by the transport, not stored in the file.
inline HcdResult NextHcdRecord(const uint8_t *data, size_t length,
                               size_t &offset, HcdRecord &record)
{
    if (offset == length) return HcdResult::End;
    if (offset > length || length - offset < 3) return HcdResult::Truncated;

    const uint8_t *header = data + offset;
    const size_t size = header[2];
    if (length - offset - 3 < size) return HcdResult::Truncated;

    record = {uint16_t(unsigned(header[0]) | (unsigned(header[1]) << 8)),
              header[2], header + 3};
    offset += 3 + size;
    return HcdResult::Record;
}
}
#endif
