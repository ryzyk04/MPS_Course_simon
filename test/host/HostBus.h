#pragma once

#include <PuzzleBus/AvrTwi.h>
#include <avr/io.h>
#include <vector>

void TWI_vect();

namespace HostBus {

inline void interrupt(uint8_t state, uint8_t value = 0)
{
    TWSR = state;
    TWDR = value;
    TWI_vect();
}

inline void deliver(const uint8_t *data, size_t size)
{
    interrupt(0x60);
    for (size_t i = 0; i < size; ++i) {
        interrupt(i < PuzzleBus::FRAME_SIZE ? 0x80 : 0x88, data[i]);
    }
    interrupt(0xA0);
}

inline std::vector<uint8_t> reply()
{
    std::vector<uint8_t> data;
    interrupt(0xA8);
    data.push_back(TWDR);
    for (size_t i = 1; i < PuzzleBus::FRAME_SIZE; ++i) {
        interrupt(0xB8);
        data.push_back(TWDR);
    }
    interrupt(0xC0);
    return data;
}

} // namespace HostBus
