#ifndef XDOS_BLE_POWER_H
#define XDOS_BLE_POWER_H

#include <circle/bcmpropertytags.h>

namespace XdosBle {
// The Pi 3B+ board maps BT_ON to firmware GPIO expander line 0.
// Call from XDOS core 0, never from an interrupt.
inline bool SetControllerPower(bool on)
{
    CBcmPropertyTags tags;
    TPropertyTagGPIOState state{};
    state.nGPIO = EXP_GPIO_BASE;
    state.nState = on ? 1 : 0;
    return tags.GetTag(PROPTAG_SET_SET_GPIO_STATE, &state, sizeof state, 8);
}
}
#endif
