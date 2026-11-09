#include "game/Puzzle.h"

namespace Simon {

uint32_t Random::next()
{
    // Формула Шраге исключает переполнение int32 и арифметику uint64.
    const int32_t high = state_ / 127773;
    const int32_t low = state_ % 127773;
    state_ = 16807 * low - 2836 * high;
    if (state_ < 0) {
        state_ += 2147483647;
    }
    return uint32_t(state_);
}

bool generateSequence(uint16_t seed, Level level, uint8_t stage,
                      Random &random, Sequence &out)
{
    if (seed > 9999 || level > HARD || stage >= STAGES) {
        return false;
    }

    static const uint8_t lengths[3][3] = {{3, 3, 4}, {3, 4, 5}, {4, 5, 6}};
    static const Color easyColors[5] = {RED, YELLOW, GREEN, BLUE, WHITE};
    Sequence sequence;
    sequence.column = pickColumn(seed, level, stage);
    sequence.length = lengths[level][stage];

    for (uint8_t i = 0; i < sequence.length; ++i) {
        const Color color = level == EASY ? easyColors[random.next() % 5]
                                          : Color(random.next() % COLOR_COUNT);
        sequence.colors[i] = color;
        sequence.buttons[i] = buttonFor(color, level, sequence.column);
    }

    out = sequence;
    return true;
}

} // namespace Simon
