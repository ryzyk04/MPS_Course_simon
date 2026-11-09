#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "uart_protocol.h"

// Протокол временного шлюза. Размер и порядок полей сохраняют совместимость
// с текущими прошивками ESP32, Саймона и лабиринта.
namespace ModuleBus {

enum Type : uint8_t { WIRES = 1, SIMON = 2, LABYRINTH = 3 };
enum Operation : uint8_t { GENERATE = 1, RESET = 2, STOP = 3, STATUS = 0x80 };
enum State : uint8_t { WAITING = 0, PLAYING = 1, SOLVED = 2, STOPPED = 3 };

constexpr uint8_t SIMON_ADDRESS = 0x11;
constexpr uint8_t LABYRINTH_ADDRESS = 0x12;
constexpr size_t FRAME_SIZE = 20;

struct Frame {
    uint8_t type = 0;
    uint8_t operation = STATUS;
    uint64_t round = 0;
    uint16_t seed = 0;
    uint8_t difficulty = 0;
    uint8_t state = WAITING;
    uint16_t errors = 0;
};

inline uint16_t crc16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFF;
    while (size--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
        }
    }
    return crc;
}

inline void encode(const Frame &frame, uint8_t *out)
{
    out[0] = 0xB7;
    out[1] = 1;
    out[2] = frame.type;
    out[3] = frame.operation;
    for (uint8_t byte = 0; byte < 8; ++byte) {
        out[4 + byte] = uint8_t(frame.round >> (8 * byte));
    }
    out[12] = uint8_t(frame.seed);
    out[13] = uint8_t(frame.seed >> 8);
    out[14] = frame.difficulty;
    out[15] = frame.state;
    out[16] = uint8_t(frame.errors);
    out[17] = uint8_t(frame.errors >> 8);
    const uint16_t crc = crc16(out, 18);
    out[18] = uint8_t(crc);
    out[19] = uint8_t(crc >> 8);
}

inline bool decode(const uint8_t *in, size_t size, Frame &out)
{
    if (!in || size != FRAME_SIZE || in[0] != 0xB7 || in[1] != 1 ||
        crc16(in, 18) != uint16_t(in[18] | uint16_t(in[19]) << 8)) {
        return false;
    }
    if (in[2] < WIRES || in[2] > LABYRINTH ||
        (in[3] != GENERATE && in[3] != RESET && in[3] != STOP && in[3] != STATUS) ||
        in[14] > 2 || in[15] > STOPPED) {
        return false;
    }

    Frame frame;
    frame.type = in[2];
    frame.operation = in[3];
    for (uint8_t byte = 0; byte < 8; ++byte) {
        frame.round |= uint64_t(in[4 + byte]) << (8 * byte);
    }
    frame.seed = uint16_t(in[12] | uint16_t(in[13]) << 8);
    frame.difficulty = in[14];
    frame.state = in[15];
    frame.errors = uint16_t(in[16] | uint16_t(in[17]) << 8);
    if (frame.seed > 9999 || (frame.operation == GENERATE && frame.round == 0)) {
        return false;
    }

    out = frame;
    return true;
}

// G:<module>:<READY|ERROR|SOLVED|ONLINE|OFFLINE>:<round>:<counter>
inline bool parseEvent(const char *line, uint8_t &module, char *event,
                       uint64_t &round, uint16_t &counter, const char *prefix = "G:")
{
    const size_t prefixSize = strlen(prefix);
    if (strlen(line) >= 48 || strncmp(line, prefix, prefixSize)) {
        return false;
    }

    char copy[48];
    strcpy(copy, line);
    char *kind = strchr(copy + prefixSize, ':');
    if (!kind) {
        return false;
    }
    *kind++ = 0;
    char *roundStart = strchr(kind, ':');
    if (!roundStart) {
        return false;
    }
    *roundStart++ = 0;
    char *value = strchr(roundStart, ':');
    if (!value) {
        return false;
    }
    *value++ = 0;

    uint8_t parsedModule;
    if (!uartParseId(copy + prefixSize, "", parsedModule) ||
        (parsedModule != SIMON && parsedModule != LABYRINTH) ||
        !uartParseId(roundStart, "", round, true) ||
        !uartParseId(value, "", counter, true)) {
        return false;
    }
    if (strcmp(kind, "READY") && strcmp(kind, "ERROR") && strcmp(kind, "SOLVED") &&
        strcmp(kind, "ONLINE") && strcmp(kind, "OFFLINE")) {
        return false;
    }

    strcpy(event, kind);
    module = parsedModule;
    return true;
}

} // namespace ModuleBus
