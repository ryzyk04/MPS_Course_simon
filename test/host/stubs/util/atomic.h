#pragma once

#include <avr/io.h>

class HostAtomicBlock {
public:
    HostAtomicBlock() : previous_(SREG) { SREG &= 0x7F; }
    ~HostAtomicBlock() { SREG = previous_; }
    explicit operator bool() const { return active_; }
    void finish() { active_ = false; }

private:
    uint8_t previous_;
    bool active_ = true;
};

#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(mode) for (HostAtomicBlock block; block; block.finish())
