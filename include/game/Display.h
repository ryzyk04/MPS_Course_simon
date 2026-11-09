#pragma once

#include "game/Game.h"

namespace Simon {

// Перерисовываются только изменённые данные. Неудачный обмен TM1637 повторяется.
class Display {
public:
    void update(const Game &game, uint32_t now);

private:
    uint8_t level_[6] = {};
    uint8_t progress_[12] = {};
    uint8_t digits_[4] = {};
    uint8_t rgb_[3] = {};
    uint32_t digitAttemptAt_ = 0;
    bool levelDirty_ = true;
    bool progressDirty_ = true;
    bool digitsDirty_ = true;
    bool first_ = true;
};

} // namespace Simon
