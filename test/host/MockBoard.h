#pragma once

#include "drivers/Board.h"
#include "module_bus.h"

#include <array>
#include <stdexcept>
#include <deque>
#include <vector>

namespace Mock {
inline uint32_t now = 0;
inline uint8_t buttons = 0;
inline uint8_t address = 0;
inline std::deque<std::array<uint8_t, 20>> commands;
inline std::array<uint8_t, 20> reply = {};
inline std::vector<uint8_t> level, progress, digits, rgb;
inline unsigned levelCalls = 0, progressCalls = 0, digitCalls = 0;
inline bool pixelsOk = true, digitsOk = true;

inline void reset()
{
    now = 0;
    buttons = address = 0;
    commands.clear();
    reply = {};
    level.clear(); progress.clear(); digits.clear(); rgb.clear();
    levelCalls = progressCalls = digitCalls = 0;
    pixelsOk = digitsOk = true;
}

inline ModuleBus::Frame status()
{
    ModuleBus::Frame frame;
    if (!ModuleBus::decode(reply.data(), reply.size(), frame)) {
        throw std::runtime_error("invalid status");
    }
    return frame;
}

inline void deliver(uint64_t round, uint16_t seed = 42, uint8_t difficulty = 0,
                    uint8_t operation = ModuleBus::GENERATE, uint8_t type = ModuleBus::SIMON)
{
    ModuleBus::Frame frame;
    frame.type = type;
    frame.operation = operation;
    frame.round = round;
    frame.seed = seed;
    frame.difficulty = difficulty;
    commands.emplace_back();
    ModuleBus::encode(frame, commands.back().data());
}
}
