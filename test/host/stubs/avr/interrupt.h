#pragma once

#include "io.h"

#define ISR(vector) void vector()

inline void cli() { SREG &= 0x7F; }
inline void sei() { SREG |= 0x80; }
