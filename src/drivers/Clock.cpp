#include "drivers/Board.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

static_assert(F_CPU == 16000000UL, "Simon requires a 16 MHz ATmega328P");

namespace {
volatile uint32_t ticks = 0;
}

ISR(TIMER1_COMPA_vect)
{
    ++ticks;
}

namespace Simon {
namespace Hardware {

void beginClock()
{
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11); // 16 МГц / 8 / 2000 = 1 кГц.
    TCNT1 = 0;
    OCR1A = 1999;
    TIFR1 = _BV(OCF1A);
    TIMSK1 = _BV(OCIE1A);
}

uint32_t millis()
{
    uint32_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        value = ticks;
    }
    return value;
}

uint32_t micros()
{
    uint32_t milliseconds;
    uint16_t fraction;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        milliseconds = ticks;
        fraction = TCNT1;
        if (TIFR1 & _BV(OCF1A)) {
            ++milliseconds;
            fraction = TCNT1;
        }
    }
    return milliseconds * 1000UL + fraction / 2;
}

} // namespace Hardware
} // namespace Simon
