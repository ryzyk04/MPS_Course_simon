#include "drivers/Board.h"
#include "Config.h"

#include <avr/io.h>

namespace Simon {
namespace Hardware {
namespace {

void channel(uint8_t value, volatile uint8_t &port, uint8_t pin,
             volatile uint8_t &control, uint8_t compareBit, volatile uint8_t &compare)
{
    const uint8_t duty = Config::COMMON_ANODE ? 255 - value : value;
    // Граничные значения задаются GPIO: PWM не даёт идеальный нулевой импульс.
    if (duty == 0 || duty == 255) {
        control &= uint8_t(~_BV(compareBit));
        if (duty) {
            port |= _BV(pin);
        } else {
            port &= uint8_t(~_BV(pin));
        }
    } else {
        compare = duty;
        control |= _BV(compareBit);
    }
}

void debugByte(uint8_t value)
{
    while (!(UCSR0A & _BV(UDRE0))) {}
    UDR0 = value;
}

} // namespace

void beginBoard()
{
    // A1..A3 и D9..D10 — кнопки с подтяжкой к питанию.
    DDRC &= uint8_t(~0x0E);
    PORTC |= 0x0E;
    DDRB &= uint8_t(~(_BV(PB1) | _BV(PB2)));
    PORTB |= _BV(PB1) | _BV(PB2);

    // Сначала безопасный уровень RGB, затем включение выходов.
    if (Config::COMMON_ANODE) {
        PORTD |= _BV(PD3) | _BV(PD5) | _BV(PD6);
    } else {
        PORTD &= uint8_t(~(_BV(PD3) | _BV(PD5) | _BV(PD6)));
    }
    DDRD |= _BV(PD3) | _BV(PD5) | _BV(PD6) | _BV(PD7);
    PORTD &= uint8_t(~_BV(PD7));
    DDRB |= _BV(PB0);
    PORTB &= uint8_t(~_BV(PB0));

    // Timer0 fast PWM на D5/D6, Timer2 phase-correct PWM на D3.
    // Источник миллисекунд вынесен на Timer1.
    TCCR0A = _BV(WGM00) | _BV(WGM01);
    TCCR0B = _BV(CS01) | _BV(CS00);
    TIMSK0 = 0;
    TCCR2A = _BV(WGM20);
    TCCR2B = _BV(CS22);
    TIMSK2 = 0;
    setRgb(0, 0, 0);

    UCSR0A = _BV(U2X0);
    UBRR0H = 0;
    UBRR0L = 16; // 115200 бод при 16 МГц, погрешность +2.1%.
    UCSR0B = _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

uint8_t readButtons()
{
    uint8_t mask = uint8_t((~PINC >> 1) & 7);
    if (!(PINB & _BV(PB1))) { mask |= 8; }
    if (!(PINB & _BV(PB2))) { mask |= 16; }
    return mask;
}

void setRgb(uint8_t red, uint8_t green, uint8_t blue)
{
    channel(red, PORTD, PD3, TCCR2A, COM2B1, OCR2B);
    channel(green, PORTD, PD5, TCCR0A, COM0B1, OCR0B);
    channel(blue, PORTD, PD6, TCCR0A, COM0A1, OCR0A);
}

void debugStage(uint8_t stage, const uint8_t *buttons, uint8_t length)
{
    if (!Config::DEBUG_SOLUTION) {
        return;
    }

    debugByte('S');
    debugByte('1' + stage);
    debugByte(':');
    for (uint8_t i = 0; i < length; ++i) {
        debugByte('0' + buttons[i]);
    }
    debugByte('\r');
    debugByte('\n');
}

} // namespace Hardware
} // namespace Simon
