#pragma once

#include <stdint.h>

namespace Simon {
namespace Hardware {

void beginBoard();
void beginClock();
uint32_t millis();
uint32_t micros();
uint8_t readButtons();
void setRgb(uint8_t red, uint8_t green, uint8_t blue);
void debugStage(uint8_t stage, const uint8_t *buttons, uint8_t length);

// Ленты получают GRB-байты с уже применённой яркостью.
enum class Strip : uint8_t { Level, Progress };
bool showPixels(Strip strip, const uint8_t *data, uint8_t size);
bool showDigits(const uint8_t segments[4]);

// Интерфейс к TWI. Размеры фиксированы старым протоколом шлюза.
void beginTwi(uint8_t address);
void publishStatus(const uint8_t *packet);
bool takeCommand(uint8_t *packet);

} // namespace Hardware
} // namespace Simon
