#pragma once

#include <stdint.h>

namespace Simon {

class Buttons {
public:
    void reset(uint8_t pressedMask, uint32_t now);
    // Только новые нажатия, без повторов от удержания или отпускания.
    uint8_t update(uint8_t pressedMask, uint32_t now);

private:
    uint8_t raw_ = 0;
    uint8_t stable_ = 0;
    uint32_t changedAt_[5] = {};
};

} // namespace Simon
