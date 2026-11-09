#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Строки до 47 символов. Повреждённая/слишком длинная строка отбрасывается
// целиком до LF, чтобы её хвост не стал отдельной командой.
struct UartLineReader {
    char buffer[48] = {};
    uint8_t length = 0;
    bool overflow = false;

    template<class Handler>
    void feed(char value, Handler handler)
    {
        if (value == '\r') {
            return;
        }
        if (value == '\n') {
            if (!overflow && length) {
                buffer[length] = 0;
                handler(buffer);
            }
            length = 0;
            overflow = false;
        } else if (!overflow) {
            if (value == 0 || length >= sizeof(buffer) - 1) {
                overflow = true;
            } else {
                buffer[length++] = value;
            }
        }
    }
};

// Числа разбираются без heap, знака и переполнения целевого типа.
// Выходной аргумент меняется после полной проверки.
template<class Id>
inline bool uartParseId(const char *line, const char *prefix, Id &id, bool allowZero = false)
{
    const size_t prefixLength = strlen(prefix);
    if (strncmp(line, prefix, prefixLength)) {
        return false;
    }

    const char *position = line + prefixLength;
    if (!*position) {
        return false;
    }

    Id value = 0;
    for (; *position; ++position) {
        if (*position < '0' || *position > '9') {
            return false;
        }
        const uint8_t digit = uint8_t(*position - '0');
        if (value > (Id(-1) - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }

    if (!value && !allowZero) {
        return false;
    }
    id = value;
    return true;
}

inline bool uartParseGeneration(const char *line, uint64_t &id,
                                uint16_t &seed, uint8_t &level)
{
    if (strlen(line) >= 48) {
        return false;
    }

    char copy[48];
    strcpy(copy, line);
    const char *prefix = "E:GENERATE:";
    if (strncmp(copy, prefix, strlen(prefix))) {
        return false;
    }

    char *seedStart = strchr(copy + strlen(prefix), ':');
    if (!seedStart) {
        return false;
    }
    *seedStart++ = 0;

    char *levelStart = strchr(seedStart, ':');
    if (!levelStart) {
        return false;
    }
    *levelStart++ = 0;

    uint64_t parsedId;
    uint16_t parsedSeed;
    uint8_t parsedLevel;
    if (!uartParseId(copy, prefix, parsedId) ||
        !uartParseId(seedStart, "", parsedSeed, true) || parsedSeed > 9999 ||
        !uartParseId(levelStart, "", parsedLevel, true) || parsedLevel > 2) {
        return false;
    }

    id = parsedId;
    seed = parsedSeed;
    level = parsedLevel;
    return true;
}
