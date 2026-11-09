#include "Test.h"
#include "HostBus.h"
#include "drivers/Board.h"
#include "TestBus.h"
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

void sharedReceive()
{
    using namespace PuzzleBus;
    Simon::Hardware::prepareTwi();
    CHECK((DDRC & 0x30) == 0 && (PORTC & 0x30) == 0x30);
    Frame command;
    command.operation = Operation::Generate;
    command.status.type = PuzzleType::Simon;
    command.status.round.id = UINT64_MAX;
    command.status.round.seed = 9999;
    command.status.round.difficulty = 2;
    uint8_t bytes[80] = {};
    CHECK(encode(command, bytes, FRAME_SIZE));

    for (size_t size = 0; size <= sizeof(bytes); ++size) {
        PuzzleNode node(PuzzleType::Simon);
        AvrTwiNode bus(node);
        CHECK(bus.begin(0x11));
        CHECK(TWAR == 0x11 * 2 && TWAMR == 0);
        HostBus::deliver(bytes, size);
        CHECK(node.status().state == PuzzleBus::State::Waiting);
        bus.update();
        CHECK((node.status().state == PuzzleBus::State::Preparing) == (size == FRAME_SIZE));
        Command received;
        CHECK(node.takeCommand(received) == (size == FRAME_SIZE));
        if (size == FRAME_SIZE) {
            CHECK(received.round.id == UINT64_MAX && received.round.seed == 9999);
            CHECK(node.ready());
            bus.update();
        } else {
            CHECK(bus.droppedPackets() == 1);
        }
    }

    PuzzleNode node(PuzzleType::Simon);
    AvrTwiNode bus(node);
    CHECK(bus.begin(0x11));
    HostBus::deliver(bytes, FRAME_SIZE);
    HostBus::deliver(bytes, FRAME_SIZE);
    CHECK(bus.droppedPackets() == 1);
    bus.update();
    Command received;
    CHECK(node.takeCommand(received));
    CHECK(!node.takeCommand(received));
    CHECK(node.ready());
    bus.update();
    HostBus::interrupt(0x60);
    HostBus::interrupt(0x80, 42);
    HostBus::interrupt(0x00);
    CHECK(bus.busErrors() == 1 && (TWCR & _BV(TWSTO)));
    HostBus::interrupt(0xA0);
    bus.update();
    CHECK(node.status().state == PuzzleBus::State::Playing);
}

void sharedTransmit()
{
    using namespace PuzzleBus;
    PuzzleNode node(PuzzleType::Simon);
    AvrTwiNode bus(node);
    CHECK(bus.begin(0x11));
    Frame frame;
    CHECK(decode(HostBus::reply().data(), FRAME_SIZE, frame) == DecodeResult::Ok);
    CHECK(frame.status.state == PuzzleBus::State::Waiting);
    Frame command;
    command.operation = Operation::Generate;
    command.status.type = PuzzleType::Simon;
    command.status.round.id = 1;
    CHECK(node.receive(command));
    Command taken;
    CHECK(node.takeCommand(taken) && node.ready());
    bus.update();
    HostBus::interrupt(0xA8);
    std::vector<uint8_t> snapshot{TWDR};
    CHECK(node.smallError());
    bus.update();
    for (size_t i = 1; i < FRAME_SIZE; ++i) {
        HostBus::interrupt(0xB8);
        snapshot.push_back(TWDR);
    }
    HostBus::interrupt(0xC0);
    CHECK(decode(snapshot.data(), snapshot.size(), frame) == DecodeResult::Ok);
    CHECK(frame.status.smallErrors == 0);
    const auto fresh = HostBus::reply();
    CHECK(decode(fresh.data(), fresh.size(), frame) == DecodeResult::Ok);
    CHECK(frame.status.smallErrors == 1);
    for (uint8_t saved : {uint8_t(0), uint8_t(0x80)}) {
        SREG = saved;
        bus.update();
        (void)bus.droppedPackets();
        (void)bus.busErrors();
        CHECK(SREG == saved);
    }
    SREG = 0x80;
}

} // namespace

int main()
{
    try {
        run("GPIO mapping and PWM all 256 duties", board);
        run("Timer1, microseconds, pending compare and interrupt state", clockChecks);
        run("TWI reception lengths 0..80, mailbox and recovery", sharedReceive);
        run("TWI immutable replies, partial reads, padding and repeated START", sharedTransmit);
        run("TM1637 start/stop, LSB frames, leading zeros and missing ACK", tm1637);
        std::cout << cases << " groups, " << checks << " checks\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << "\n";
        return 1;
    }
}
