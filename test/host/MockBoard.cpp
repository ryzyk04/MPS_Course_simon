#include <stdexcept>
#include "MockBoard.h"

#include <algorithm>

namespace Simon {
namespace Hardware {

void beginBoard() {}
void beginClock() {}
uint32_t millis() { Mock::pump(); return Mock::now; }
uint32_t micros() { return Mock::now * 1000; }
uint8_t readButtons() { return Mock::buttons; }
void debugStage(uint8_t, const uint8_t *, uint8_t) {}
void prepareTwi() { Mock::address = 0x11; }

void setRgb(uint8_t r, uint8_t g, uint8_t b)
{
    Mock::rgb = {r, g, b};
}

bool showDigits(const uint8_t *segments)
{
    ++Mock::digitCalls;
    Mock::digits.assign(segments, segments + 4);
    return Mock::digitsOk;
}

bool showPixels(Strip strip, const uint8_t *data, uint8_t size)
{
    if (strip == Strip::Level) {
        ++Mock::levelCalls;
        Mock::level.assign(data, data + size);
    } else {
        ++Mock::progressCalls;
        Mock::progress.assign(data, data + size);
    }
    return Mock::pixelsOk;
}

} // namespace Hardware
} // namespace Simon
