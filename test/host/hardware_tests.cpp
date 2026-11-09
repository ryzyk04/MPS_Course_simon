#include "Test.h"
#include "drivers/Board.h"
#include "module_bus.h"
#include "Config.h"

#include <avr/io.h>
#include <vector>
#include <array>
#include <cstring>

void TWI_vect();
void TIMER1_COMPA_vect();

namespace Host {
std::vector<std::pair<bool, bool>> pins;
void delay()
{
    // Моделируем внешние подтяжки CLK/DIO, при этом PIND задаёт ACK устройства.
    CHECK((PORTD & (_BV(PD2) | _BV(PD4))) == 0);
    pins.emplace_back(!(DDRD & _BV(PD2)), !(DDRD & _BV(PD4)));
}
}

using namespace Simon::Hardware;

namespace {

void interrupt(uint8_t status, uint8_t data = 0)
{
    TWSR = status;
    TWDR = data;
    TWI_vect();
}

void send(const uint8_t *data, size_t size, uint8_t addressStatus = 0x60)
{
    interrupt(addressStatus);
    for (size_t i = 0; i < size; ++i) {
        interrupt(i < 20 ? 0x80 : 0x88, data[i]);
    }
    interrupt(0xA0);
}

std::vector<uint8_t> read(size_t length)
{
    interrupt(0xA8);
    std::vector<uint8_t> out;
    for (size_t i = 0; i < length; ++i) {
        out.push_back(TWDR);
        if (i + 1 < length) {
            interrupt(0xB8);
        }
    }
    interrupt(0xC0);
    return out;
}

void board()
{
    DDRC = DDRB = DDRD = 0;
    PORTC = PORTB = PORTD = 0;
    beginBoard();
    CHECK((DDRC & 0x0E) == 0 && (PORTC & 0x0E) == 0x0E);
    CHECK((DDRB & 6) == 0 && (PORTB & 6) == 6);
    CHECK((DDRB & 1) != 0 && (PORTB & 1) == 0);
    CHECK((DDRD & 0xE8) == 0xE8);
    CHECK(TCCR0A == (_BV(WGM00) | _BV(WGM01)));
    CHECK(TCCR0B == (_BV(CS01) | _BV(CS00)) && TIMSK0 == 0);
    CHECK(TCCR2A == _BV(WGM20) && TCCR2B == _BV(CS22) && TIMSK2 == 0);
    CHECK(UBRR0L == 16 && UBRR0H == 0 && UCSR0B == _BV(TXEN0));

    for (unsigned mask = 0; mask < 32; ++mask) {
        PINC = uint8_t(~((mask & 7) << 1));
        PINB = uint8_t(~(((mask >> 3) & 3) << 1));
        CHECK(readButtons() == mask);
    }

    for (unsigned value = 0; value < 256; ++value) {
        setRgb(value, value, value);
        if (value == 0 || value == 255) {
            CHECK(!(TCCR0A & (_BV(COM0A1) | _BV(COM0B1))));
            CHECK(!(TCCR2A & _BV(COM2B1)));
            CHECK(bool(PORTD & _BV(PD3)) == (value == 0));
            CHECK(bool(PORTD & _BV(PD5)) == (value == 0));
            CHECK(bool(PORTD & _BV(PD6)) == (value == 0));
        } else {
            CHECK(OCR0A == 255 - value && OCR0B == 255 - value && OCR2B == 255 - value);
            CHECK((TCCR0A & 0xA0) == 0xA0 && (TCCR2A & 0x20));
        }
    }
    setRgb(1, 2, 3);
    CHECK(OCR2B == 254 && OCR0B == 253 && OCR0A == 252);
}

void clockChecks()
{
    TIFR1.raise(0xFF);
    beginClock();
    CHECK(OCR1A == 1999 && TCNT1 == 0);
    CHECK(TCCR1B == (_BV(WGM12) | _BV(CS11)) && TIMSK1 == _BV(OCIE1A));
    CHECK(!(TIFR1 & _BV(OCF1A)));

    const uint32_t base = millis();
    TCNT1 = 1998;
    CHECK(micros() == base * 1000 + 999);
    TIFR1.raise(_BV(OCF1A));
    TCNT1 = 8;
    CHECK(micros() == (base + 1) * 1000 + 4);
    TIFR1 = _BV(OCF1A);
    TIMER1_COMPA_vect();
    CHECK(millis() == base + 1);
    for (uint8_t saved : {uint8_t(0), uint8_t(0x80)}) {
        SREG = saved;
        (void)millis(); (void)micros();
        CHECK(SREG == saved);
    }
    SREG = 0x80;
}

void slaveReceive()
{
    beginTwi(0x11);
    CHECK(TWAR == 0x22 && TWAMR == 0);
    CHECK((TWCR & (_BV(TWIE) | _BV(TWEN) | _BV(TWEA))) ==
          (_BV(TWIE) | _BV(TWEN) | _BV(TWEA)));
    uint8_t bytes[80];
    for (size_t i = 0; i < sizeof(bytes); ++i) { bytes[i] = i; }
    uint8_t packet[20] = {};

    for (size_t length = 0; length <= sizeof(bytes); ++length) {
        send(bytes, length);
        CHECK(takeCommand(packet) == (length == 20));
        if (length == 20) { CHECK(!memcmp(bytes, packet, 20)); }
        CHECK(!takeCommand(packet));
    }

    send(bytes, 20, 0x68);
    CHECK(!takeCommand(nullptr)); // Не съедает готовый пакет.
    CHECK(takeCommand(packet));
    CHECK(!memcmp(bytes, packet, 20));
    // Пакет публикуется только после STOP, CRC проверяет основной цикл.
    interrupt(0x60);
    for (uint8_t i = 0; i < 20; ++i) { interrupt(0x80, bytes[i]); }
    CHECK(!takeCommand(packet));
    CHECK(!(TWCR & _BV(TWEA))); // 21-й байт будет принят с NACK.
    interrupt(0xA0);
    CHECK(takeCommand(packet));

    // Непрочитанный пакет не затирается следующим. Шлюз повторит команду.
    send(bytes, 20);
    bytes[0] = 200;
    send(bytes, 20);
    CHECK(takeCommand(packet) && packet[0] == 0);
    CHECK(!takeCommand(packet));

    interrupt(0x60);
    interrupt(0x80, 42);
    interrupt(0x00); // Ошибка отменяет незавершённую запись.
    CHECK(TWCR & _BV(TWSTO));
    interrupt(0xA0);
    CHECK(!takeCommand(packet));
    send(bytes, 20);
    CHECK(takeCommand(packet) && packet[0] == 200);

    interrupt(0x60);
    interrupt(0x80, 42);
    interrupt(0xF8);
    interrupt(0xA0);
    CHECK(!takeCommand(packet));
}

void slaveTransmit()
{
    uint8_t first[20], second[20];
    for (uint8_t i = 0; i < 20; ++i) {
        first[i] = i + 10;
        second[i] = i + 100;
    }
    publishStatus(first);
    interrupt(0xA8);
    CHECK(TWDR == first[0]);
    publishStatus(second);
    for (uint8_t i = 1; i < 20; ++i) {
        interrupt(0xB8);
        CHECK(TWDR == first[i]); // Обновление не рвёт кадр посередине.
    }
    for (uint8_t i = 0; i < 20; ++i) {
        interrupt(0xB8);
        CHECK(TWDR == 0);
    }
    interrupt(0xC8);
    CHECK(read(20) == std::vector<uint8_t>(second, second + 20));
    CHECK(read(1)[0] == second[0]);
    CHECK(read(20) == std::vector<uint8_t>(second, second + 20));
    interrupt(0xB0);
    CHECK(TWDR == second[0]);
    interrupt(0xC0);

    // repeated START: готовая команда и параллельный ответ независимы.
    send(first, 20);
    CHECK(read(20) == std::vector<uint8_t>(second, second + 20));
    uint8_t packet[20];
    CHECK(takeCommand(packet) && !memcmp(packet, first, 20));
    for (uint8_t saved : {uint8_t(0), uint8_t(0x80)}) {
        SREG = saved;
        publishStatus(first);
        (void)takeCommand(packet);
        beginTwi(0x11);
        CHECK(SREG == saved);
    }
    SREG = 0x80;
}

std::vector<std::vector<uint8_t>> decodeDisplayTrace()
{
    std::vector<std::vector<uint8_t>> frames;
    bool clock = true, data = true, active = false;
    std::vector<bool> bits;
    for (const auto &pin : Host::pins) {
        const bool nextClock = pin.first, nextData = pin.second;
        if (clock && nextClock && data && !nextData) {
            active = true;
            bits.clear();
        }
        if (active && !clock && nextClock) {
            bits.push_back(nextData);
        }
        if (active && clock && nextClock && !data && nextData) {
            std::vector<uint8_t> bytes;
            // Последний CLK перед STOP — служебный, не часть байта.
            CHECK(bits.size() % 9 == 1);
            for (size_t start = 0; start + 8 < bits.size(); start += 9) {
                uint8_t value = 0;
                for (uint8_t bit = 0; bit < 8; ++bit) {
                    value |= uint8_t(bits[start + bit]) << bit;
                }
                bytes.push_back(value);
                CHECK(bits[start + 8]); // DIO отпущен на время ACK.
            }
            frames.push_back(bytes);
            active = false;
        }
        clock = nextClock;
        data = nextData;
    }
    CHECK(!active);
    return frames;
}

void tm1637()
{
    PIND = 0; // Подтверждение всех байтов.
    DDRD &= uint8_t(~(_BV(PD2) | _BV(PD4)));
    PORTD &= uint8_t(~(_BV(PD2) | _BV(PD4)));
    Host::pins.clear();
    const uint8_t segments[] = {0x3F, 0x3F, 0x66, 0x5B};
    SREG = 0x80;
    CHECK(showDigits(segments));
    CHECK(SREG == 0x80);
    const auto frames = decodeDisplayTrace();
    CHECK(frames.size() == 3);
    CHECK(frames[0] == std::vector<uint8_t>({0x40}));
    CHECK(frames[1] == std::vector<uint8_t>({0xC0, 0x3F, 0x3F, 0x66, 0x5B}));
    CHECK(frames[2] == std::vector<uint8_t>({0x8D}));
    CHECK(!(DDRD & (_BV(PD2) | _BV(PD4))));

    PIND = _BV(PD4); // Нет дисплея / NACK: выход без зависания.
    Host::pins.clear();
    CHECK(!showDigits(segments));
    CHECK(decodeDisplayTrace().size() == 1);
    CHECK(!showDigits(nullptr));
    CHECK(SREG == 0x80);
}

} // namespace

int main()
{
    try {
        run("GPIO mapping and PWM all 256 duties", board);
        run("Timer1, microseconds, pending compare and interrupt state", clockChecks);
        run("TWI reception lengths 0..80, mailbox and recovery", slaveReceive);
        run("TWI immutable replies, partial reads, padding and repeated START", slaveTransmit);
        run("TM1637 start/stop, LSB frames, leading zeros and missing ACK", tm1637);
        std::cout << cases << " groups, " << checks << " checks\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << "\n";
        return 1;
    }
}
