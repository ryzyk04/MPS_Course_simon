#pragma once

#include <PuzzleBus/Protocol.h>
#include <PuzzleBus/UartProtocol.h>

// Fixture for existing game tests. Production code uses PuzzleBus DTOs directly.
// This builder can deliberately emit invalid fields with a valid CRC.
namespace TestBus {
constexpr uint8_t WIRES = 1, SIMON = 2, LABYRINTH = 3;
constexpr uint8_t GENERATE = uint8_t(PuzzleBus::Operation::Generate);
constexpr uint8_t STOP = uint8_t(PuzzleBus::Operation::Stop);
constexpr uint8_t RESET = uint8_t(PuzzleBus::Operation::Reset);
constexpr uint8_t STATUS = uint8_t(PuzzleBus::Operation::Status);
constexpr uint8_t WAITING = uint8_t(PuzzleBus::State::Waiting);
constexpr uint8_t PREPARING = uint8_t(PuzzleBus::State::Preparing);
constexpr uint8_t PLAYING = uint8_t(PuzzleBus::State::Playing);
constexpr uint8_t SOLVED = uint8_t(PuzzleBus::State::Solved);
constexpr uint8_t STOPPED = uint8_t(PuzzleBus::State::Stopped);
constexpr size_t FRAME_SIZE = PuzzleBus::FRAME_SIZE;
struct Frame {
    uint8_t type = 0, operation = STATUS;
    uint64_t round = 0;
    uint16_t seed = 0;
    uint8_t difficulty = 0, state = WAITING;
    uint16_t errors = 0;
};
inline void encode(const Frame &f, uint8_t *out)
{
    out[0] = 0xB7; out[1] = PuzzleBus::PROTOCOL_VERSION;
    out[2] = f.operation; out[3] = f.type;
    for (uint8_t i = 0; i < 8; ++i) { out[4+i] = uint8_t(f.round >> (8*i)); }
    out[12] = uint8_t(f.seed); out[13] = uint8_t(f.seed >> 8);
    out[14] = f.difficulty; out[15] = f.state;
    out[16] = uint8_t(f.errors); out[17] = uint8_t(f.errors >> 8);
    out[18] = out[19] = 0;
    const uint16_t crc = PuzzleBus::crc16(out, 20);
    out[20] = uint8_t(crc); out[21] = uint8_t(crc >> 8);
}
inline bool decode(const uint8_t *data, size_t size, Frame &out)
{
    PuzzleBus::Frame f;
    if (PuzzleBus::decode(data, size, f) != PuzzleBus::DecodeResult::Ok) { return false; }
    out.type = uint8_t(f.status.type); out.operation = uint8_t(f.operation);
    out.round = f.status.round.id; out.seed = f.status.round.seed;
    out.difficulty = f.status.round.difficulty; out.state = uint8_t(f.status.state);
    out.errors = f.status.smallErrors;
    return true;
}
using PuzzleBus::GatewayUart::parseEvent;
}
