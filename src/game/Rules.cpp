#include "game/Puzzle.h"

namespace Simon {
namespace {

// Номера кнопок 1..5. Нулевая ячейка означает цвет вне лёгкой палитры.
const uint8_t easyTable[COLOR_COUNT][2] = {
    {4, 3}, {0, 0}, {3, 5}, {0, 0}, {1, 2},
    {5, 4}, {0, 0}, {2, 1}, {0, 0},
};

const uint8_t mediumTable[COLOR_COUNT][4] = {
    {2, 4, 1, 5}, {5, 1, 3, 2}, {1, 3, 5, 4},
    {4, 2, 3, 1}, {3, 5, 2, 4}, {4, 3, 1, 2},
    {1, 5, 4, 3}, {3, 2, 5, 1}, {5, 1, 4, 3},
};

const uint8_t hardTable[COLOR_COUNT][6] = {
    {3, 5, 1, 4, 2, 3}, {1, 4, 3, 2, 5, 2}, {4, 2, 5, 1, 3, 4},
    {2, 1, 5, 3, 4, 5}, {5, 3, 2, 4, 1, 1}, {2, 4, 4, 5, 1, 3},
    {4, 2, 3, 1, 5, 1}, {5, 5, 2, 3, 4, 2}, {1, 3, 4, 5, 2, 4},
};

bool isPrime(uint8_t digit)
{
    return digit == 2 || digit == 3 || digit == 5 || digit == 7;
}

struct Digits {
    uint8_t d[4];

    explicit Digits(uint16_t seed)
    {
        for (int8_t i = 3; i >= 0; --i) {
            d[i] = seed % 10;
            seed /= 10;
        }
    }

    uint8_t sum() const { return d[0] + d[1] + d[2] + d[3]; }
    bool firstHalfBigger() const { return d[0] + d[1] > d[2] + d[3]; }

    bool contains(uint8_t value) const
    {
        for (uint8_t digit : d) {
            if (digit == value) {
                return true;
            }
        }
        return false;
    }

    bool repeats() const
    {
        for (uint8_t i = 0; i < 4; ++i) {
            for (uint8_t j = i + 1; j < 4; ++j) {
                if (d[i] == d[j]) {
                    return true;
                }
            }
        }
        return false;
    }

    uint8_t odds() const
    {
        uint8_t count = 0;
        for (uint8_t digit : d) {
            count += digit % 2;
        }
        return count;
    }

    bool containsPrime() const
    {
        for (uint8_t digit : d) {
            if (isPrime(digit)) {
                return true;
            }
        }
        return false;
    }
};

uint8_t mediumColumn(const Digits &n, uint8_t stage)
{
    const bool sumEven = n.sum() % 2 == 0;
    const bool halfBigger = n.firstHalfBigger();

    if (stage == 0) {
        return sumEven ? (n.d[0] > n.d[3] ? 0 : 1) : (n.d[0] > n.d[3] ? 2 : 3);
    }
    if (stage == 1) {
        return n.d[1] % 2 == 0 ? (halfBigger ? 3 : 0) : (halfBigger ? 1 : 2);
    }
    if (n.d[2] > n.d[1]) {
        return n.sum() > 18 ? 1 : 2;
    }
    return n.sum() > 18 ? 3 : 0;
}

uint8_t hardColumn(const Digits &n, uint8_t stage)
{
    // Порядок условий важен: используется первое подходящее правило.
    if (stage == 0) {
        if (n.repeats()) {
            return 3;
        }
        if (n.sum() > 20) {
            return 5;
        }
        if (n.contains(0)) {
            return 1;
        }
        if (n.firstHalfBigger()) {
            return 4;
        }
        if (isPrime(n.d[3])) {
            return 0;
        }
        return 2;
    }

    if (stage == 1) {
        if (n.d[0] > n.d[3] && n.sum() % 2 == 0) {
            return 2;
        }
        if (n.odds() == 2) {
            return 0;
        }
        if (isPrime(n.d[1])) {
            return 4;
        }
        if (n.sum() < 15) {
            return 5;
        }
        if (n.contains(9)) {
            return 1;
        }
        return 3;
    }

    if (n.d[2] > n.d[1] && n.d[3] % 2 == 1) {
        return 5;
    }
    if (n.d[1] + n.d[2] > 10) {
        return 3;
    }
    if (!n.containsPrime()) {
        return 0;
    }
    if (n.d[0] % 2 == 0) {
        return 1;
    }
    if (n.contains(1)) {
        return 2;
    }
    return 4;
}

} // namespace

uint8_t pickColumn(uint16_t seed, Level level, uint8_t stage)
{
    if (seed > 9999 || level > HARD || stage >= STAGES) {
        return 0xFF;
    }

    const Digits digits(seed);
    switch (level) {
    case EASY: return digits.d[3] % 2;
    case MEDIUM: return mediumColumn(digits, stage);
    case HARD: return hardColumn(digits, stage);
    }
    return 0xFF;
}

uint8_t buttonFor(Color color, Level level, uint8_t column)
{
    const uint8_t columns[] = {2, 4, 6};
    if (level > HARD || color >= COLOR_COUNT || column >= columns[level]) {
        return 0;
    }

    switch (level) {
    case EASY: return easyTable[color][column];
    case MEDIUM: return mediumTable[color][column];
    case HARD: return hardTable[color][column];
    }
    return 0;
}

} // namespace Simon
