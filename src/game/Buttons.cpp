#include "game/Buttons.h"
#include "Config.h"

namespace Simon {

void Buttons::reset(uint8_t pressedMask, uint32_t now)
{
    raw_ = stable_ = pressedMask & 0x1F;
    for (uint8_t i = 0; i < 5; ++i) {
        changedAt_[i] = now;
    }
}

uint8_t Buttons::update(uint8_t pressedMask, uint32_t now)
{
    uint8_t presses = 0;
    for (uint8_t i = 0; i < 5; ++i) {
        const uint8_t bit = uint8_t(1U << i);
        if ((pressedMask ^ raw_) & bit) {
            raw_ ^= bit;
            changedAt_[i] = now;
        }
        if (((raw_ ^ stable_) & bit) &&
            uint32_t(now - changedAt_[i]) >= Config::DEBOUNCE_MS) {
            stable_ ^= bit;
            presses |= stable_ & bit;
        }
    }
    return presses;
}

} // namespace Simon
