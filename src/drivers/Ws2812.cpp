#include "drivers/Board.h"
#include "Config.h"

#include <avr/io.h>
#include <avr/interrupt.h>

static_assert(F_CPU == 16000000UL, "WS2812 timing requires 16 MHz");

namespace {

// Каждый бит занимает 20 тактов. HIGH: 5 тактов для 0, 11 для 1.
// Для TWI задержка ISR безопасна: аппаратный slave удерживает SCL до обработки.
#define PIXEL_BIT(bit) \
    "out %[port], %[high]\n\t" \
    "nop\n\tnop\n\tnop\n\t" \
    "sbrs %[value], " #bit "\n\t" \
    "out %[port], %[low]\n\t" \
    "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\t" \
    "out %[port], %[low]\n\t"

#define PADDING "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"

template<bool Level>
void transmit(const uint8_t *data, uint8_t remaining, uint8_t high, uint8_t low)
{
    uint8_t value;
    asm volatile(
        "ld %[value], Z+\n\t"
        "1:\n\t"
        PIXEL_BIT(7) PADDING
        PIXEL_BIT(6) PADDING
        PIXEL_BIT(5) PADDING
        PIXEL_BIT(4) PADDING
        PIXEL_BIT(3) PADDING
        PIXEL_BIT(2) PADDING
        PIXEL_BIT(1) PADDING
        PIXEL_BIT(0)
        // 8 тактов до следующего бита: nop+nop+dec+breq+ld+rjmp.
        "nop\n\tnop\n\t"
        "dec %[remaining]\n\t"
        "breq 2f\n\t"
        "ld %[value], Z+\n\t"
        "rjmp 1b\n\t"
        "2:\n\t"
        : [data] "+&z" (data), [remaining] "+&r" (remaining), [value] "=&r" (value)
        : [port] "I" (Level ? _SFR_IO_ADDR(PORTD) : _SFR_IO_ADDR(PORTB)),
          [high] "r" (high), [low] "r" (low)
        : "memory", "cc"
    );
}

#undef PIXEL_BIT
#undef PADDING

} // namespace

namespace Simon {
namespace Hardware {

bool showPixels(Strip strip, const uint8_t *data, uint8_t size)
{
    static uint32_t lastLevel = 0;
    static uint32_t lastProgress = 0;
    const bool level = strip == Strip::Level;
    if (!data || (level ? size != 6 : size != 12)) {
        return false;
    }

    uint32_t &last = level ? lastLevel : lastProgress;
    if (uint32_t(micros() - last) < Config::LED_RESET_US) {
        return false;
    }

    const uint8_t saved = SREG;
    cli();
    if (level) {
        const uint8_t low = PORTD & uint8_t(~_BV(PD7));
        transmit<true>(data, size, low | _BV(PD7), low);
    } else {
        const uint8_t low = PORTB & uint8_t(~_BV(PB0));
        transmit<false>(data, size, low | _BV(PB0), low);
    }
    SREG = saved;
    last = micros();
    return true;
}

} // namespace Hardware
} // namespace Simon
