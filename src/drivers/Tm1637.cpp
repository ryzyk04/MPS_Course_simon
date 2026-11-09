#include "drivers/Board.h"
#include "Config.h"

#include <avr/io.h>
#include <util/delay.h>

namespace {

// Оба сигнала открытый сток: LOW задаётся выходом, HIGH — отпусканием линии.
void clock(bool high)
{
    if (high) { DDRD &= uint8_t(~_BV(PD2)); }
    else { DDRD |= _BV(PD2); }
    _delay_us(Simon::Config::TM_BIT_US);
}

void data(bool high)
{
    if (high) { DDRD &= uint8_t(~_BV(PD4)); }
    else { DDRD |= _BV(PD4); }
    _delay_us(Simon::Config::TM_BIT_US);
}

void start()
{
    data(true);
    clock(true);
    data(false);
    clock(false);
}

void stop()
{
    clock(false);
    data(false);
    clock(true);
    data(true);
}

bool write(uint8_t value)
{
    for (uint8_t bit = 0; bit < 8; ++bit) {
        clock(false);
        data(value & 1);
        clock(true);
        value >>= 1;
    }

    clock(false);
    data(true);
    clock(true);
    const bool acknowledged = !(PIND & _BV(PD4));
    clock(false);
    return acknowledged;
}

bool command(uint8_t value)
{
    start();
    const bool acknowledged = write(value);
    stop();
    return acknowledged;
}

} // namespace

namespace Simon {
namespace Hardware {

bool showDigits(const uint8_t segments[4])
{
    if (!segments) {
        return false;
    }

    PORTD &= uint8_t(~(_BV(PD2) | _BV(PD4)));
    // Прерывания остаются включены: TM1637 допускает растяжение импульсов.
    if (!command(0x40)) {
        return false;
    }

    start();
    bool acknowledged = write(0xC0);
    for (uint8_t i = 0; i < 4; ++i) {
        acknowledged = write(segments[i]) && acknowledged;
    }
    stop();
    return command(0x88 | Config::DISPLAY_BRIGHTNESS) && acknowledged;
}

} // namespace Hardware
} // namespace Simon
