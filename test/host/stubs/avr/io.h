#pragma once

#include <stdint.h>

#define _BV(bit) (1U << (bit))
#define PB0 0
#define PB1 1
#define PB2 2
#define PD2 2
#define PD3 3
#define PD4 4
#define PD5 5
#define PD6 6
#define PD7 7
#define PC4 4
#define PC5 5
#define WGM00 0
#define WGM01 1
#define WGM12 3
#define WGM20 0
#define CS00 0
#define CS01 1
#define CS11 1
#define CS22 2
#define COM0A1 7
#define COM0B1 5
#define COM2B1 5
#define OCF1A 1
#define OCIE1A 1
#define U2X0 1
#define UDRE0 5
#define TXEN0 3
#define UCSZ00 1
#define UCSZ01 2
#define TWINT 7
#define TWEA 6
#define TWSTO 4
#define TWEN 2
#define TWIE 0

inline uint8_t SREG = 0x80;
inline uint8_t DDRB = 0, DDRC = 0, DDRD = 0;
inline uint8_t PORTB = 0, PORTC = 0, PORTD = 0;
inline uint8_t PINB = 0xFF, PINC = 0xFF, PIND = 0;
inline uint8_t TCCR0A = 0, TCCR0B = 0, TIMSK0 = 0, OCR0A = 0, OCR0B = 0;
inline uint8_t TCCR2A = 0, TCCR2B = 0, TIMSK2 = 0, OCR2B = 0;
inline uint8_t TCCR1A = 0, TCCR1B = 0, TIMSK1 = 0;
inline uint16_t TCNT1 = 0, OCR1A = 0;
inline uint8_t UCSR0A = 0, UCSR0B = 0, UCSR0C = 0, UBRR0H = 0, UBRR0L = 0, UDR0 = 0;
inline uint8_t TWSR = 0, TWDR = 0, TWCR = 0, TWAR = 0, TWAMR = 0;

struct FlagRegister {
    uint8_t value = 0;
    operator uint8_t() const { return value; }
    void operator=(uint8_t clear) { value &= uint8_t(~clear); }
    void raise(uint8_t flags) { value |= flags; }
};
inline FlagRegister TIFR1;

namespace Host {
void delay();
}
