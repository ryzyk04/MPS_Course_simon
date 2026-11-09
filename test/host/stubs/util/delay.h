#pragma once

#include <avr/io.h>

inline void _delay_us(double)
{
    Host::delay();
}
