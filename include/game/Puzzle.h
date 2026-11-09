#pragma once

#include <stdint.h>

namespace Simon {

enum Color : uint8_t { WHITE, PINK, RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, PURPLE, COLOR_COUNT };
enum Level : uint8_t { EASY, MEDIUM, HARD };
constexpr uint8_t STAGES = 3;
constexpr uint8_t MAX_SEQUENCE = 6;
constexpr uint8_t NO_COLOR = 0xFF;

struct Sequence {
    uint8_t column = 0;
    uint8_t length = 0;
    uint8_t colors[MAX_SEQUENCE] = {};
    uint8_t buttons[MAX_SEQUENCE] = {};
};

// Состояние AVR libc random(seed + 1) продолжается между этапами.
class Random {
public:
    explicit Random(uint16_t seed = 0) : state_(uint32_t(seed) + 1) {}
    uint32_t next();

private:
    int32_t state_;
};

uint8_t pickColumn(uint16_t seed, Level level, uint8_t stage);
uint8_t buttonFor(Color color, Level level, uint8_t column);
bool generateSequence(uint16_t seed, Level level, uint8_t stage,
                      Random &random, Sequence &out);

} // namespace Simon
