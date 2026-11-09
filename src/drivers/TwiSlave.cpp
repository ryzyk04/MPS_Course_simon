#include "drivers/Board.h"
#include "module_bus.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <string.h>

namespace {

constexpr uint8_t size = ModuleBus::FRAME_SIZE;
uint8_t receiving[size] = {};
uint8_t pending[size] = {};
uint8_t reply[size] = {};
uint8_t sending[size] = {};
uint8_t received = 0;
uint8_t sent = 0;
bool writing = false;
volatile bool available = false;

void listen()
{
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
}

} // namespace

ISR(TWI_vect)
{
    switch (TWSR & 0xF8) {
    case 0x60: // SLA+W, включая потерю арбитража при адресации.
    case 0x68:
        received = 0;
        writing = true;
        break;

    case 0x80: // Получен байт с ACK.
    case 0x88: // Лишний байт уже принят с NACK.
        if (writing && received < size) {
            receiving[received] = TWDR;
        }
        if (received <= size) {
            ++received; // size + 1 запоминает переполнение до STOP.
        }
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) |
               (received < size ? _BV(TWEA) : 0);
        return;

    case 0xA0: // STOP либо repeated START завершает запись.
        if (writing && received == size && !available) {
            memcpy(pending, receiving, size);
            available = true;
        }
        writing = false;
        break;

    case 0xA8: // SLA+R. Снимок остаётся неизменным до конца чтения.
    case 0xB0:
        writing = false;
        memcpy(sending, reply, size);
        sent = 0;
        TWDR = sending[sent++];
        break;

    case 0xB8:
        // Чтение за пределами кадра заполняется нулями, память не раскрывается.
        TWDR = sent < size ? sending[sent++] : 0;
        break;

    case 0xC0: // Мастер завершил чтение NACK.
    case 0xC8:
        break;

    case 0x00: // Ошибка шины: сбрасываем незавершённую запись.
        writing = false;
        received = 0;
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
        return;

    default:
        writing = false;
        break;
    }
    listen();
}

namespace Simon {
namespace Hardware {

void beginTwi(uint8_t address)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        available = false;
        received = sent = 0;
        writing = false;
        DDRC &= uint8_t(~(_BV(PC4) | _BV(PC5)));
        PORTC |= _BV(PC4) | _BV(PC5);
        TWAR = address << 1; // Общий вызов отключён.
        TWAMR = 0;
        listen();
    }
}

void publishStatus(const uint8_t *packet)
{
    if (!packet) {
        return;
    }
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        memcpy(reply, packet, size);
    }
}

bool takeCommand(uint8_t *packet)
{
    if (!packet) {
        return false;
    }
    bool found = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (available) {
            memcpy(packet, pending, size);
            available = false;
            found = true;
        }
    }
    return found;
}

} // namespace Hardware
} // namespace Simon
