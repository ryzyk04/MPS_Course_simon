#include "game/Display.h"
#include "drivers/Board.h"
#include "Config.h"

#include <string.h>

namespace Simon {
namespace {

const uint8_t palette[COLOR_COUNT][3] = {
    {255, 255, 255}, {255, 0, 51}, {255, 0, 0},
    {255, 51, 0}, {255, 180, 0}, {0, 255, 0},
    {0, 200, 255}, {0, 0, 255}, {162, 0, 230},
};

void pixel(uint8_t *out, uint8_t red, uint8_t green, uint8_t blue)
{
    // NeoPixel использовал множитель brightness + 1 с делением на 256.
    out[0] = uint16_t(green) * (uint16_t(Config::STRIP_BRIGHTNESS) + 1) / 256;
    out[1] = uint16_t(red) * (uint16_t(Config::STRIP_BRIGHTNESS) + 1) / 256;
    out[2] = uint16_t(blue) * (uint16_t(Config::STRIP_BRIGHTNESS) + 1) / 256;
}

bool changed(uint8_t *saved, const uint8_t *data, uint8_t size)
{
    if (!memcmp(saved, data, size)) {
        return false;
    }
    memcpy(saved, data, size);
    return true;
}

} // namespace

void Display::update(const Game &game, uint32_t now)
{
    uint8_t level[6] = {};
    uint8_t progress[12] = {};
    uint8_t digits[4] = {};
    uint8_t rgb[3] = {};
    const bool visible = game.state() != State::Waiting;

    if (visible) {
        static const uint8_t segments[10] = {
            0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
        };
        uint16_t seed = game.seed();
        for (int8_t i = 3; i >= 0; --i) {
            digits[i] = segments[seed % 10];
            seed /= 10;
        }

        const uint8_t levelColors[3][3] = {{0, 255, 0}, {255, 180, 0}, {255, 0, 0}};
        const uint8_t *color = levelColors[game.level()];
        pixel(level, color[0], color[1], color[2]);

        for (uint8_t i = 0; i < STAGES; ++i) {
            if (i < game.stage()) {
                pixel(progress + 3 * i, 0, 255, 0);
            } else if (i == game.stage()) {
                if (game.errorActive()) {
                    pixel(progress + 3 * i, 255, 0, 0);
                } else if ((now / Config::STAGE_BLINK_MS) % 2 == 0) {
                    pixel(progress + 3 * i, 255, 180, 0);
                }
            }
        }

        if (game.shownColor() < COLOR_COUNT) {
            for (uint8_t i = 0; i < 3; ++i) {
                rgb[i] = uint16_t(palette[game.shownColor()][i]) * Config::RGB_BRIGHTNESS / 255;
            }
        }
    }

    if (changed(rgb_, rgb, sizeof(rgb)) || first_) {
        Hardware::setRgb(rgb[0], rgb[1], rgb[2]);
    }
    levelDirty_ |= changed(level_, level, sizeof(level));
    progressDirty_ |= changed(progress_, progress, sizeof(progress));
    const bool digitsChanged = changed(digits_, digits, sizeof(digits));
    digitsDirty_ |= digitsChanged;

    if (digitsDirty_ && (first_ || digitsChanged ||
                        uint32_t(now - digitAttemptAt_) >= Config::DISPLAY_RETRY_MS)) {
        digitAttemptAt_ = now;
        digitsDirty_ = !Hardware::showDigits(digits_);
    }
    if (levelDirty_) {
        levelDirty_ = !Hardware::showPixels(Hardware::Strip::Level, level_, sizeof(level_));
    }
    if (progressDirty_) {
        progressDirty_ = !Hardware::showPixels(Hardware::Strip::Progress, progress_, sizeof(progress_));
    }
    first_ = false;
}

} // namespace Simon
