#pragma once

#include <stdint.h>

namespace Simon {
namespace Config {

// Интервалы в миллисекундах, кроме явно обозначенных микросекунд.
constexpr uint16_t FLASH_ON_MS = 500;
constexpr uint16_t FLASH_OFF_MS = 250;
constexpr uint16_t SHOW_PAUSE_MS = 2000;
constexpr uint16_t STAGE_DONE_MS = 800;
constexpr uint16_t ERROR_MS = 500;
constexpr uint16_t STAGE_BLINK_MS = 400;
constexpr uint8_t DEBOUNCE_MS = 30;

constexpr bool COMMON_ANODE = true;
constexpr uint8_t RGB_BRIGHTNESS = 30;
constexpr uint8_t STRIP_BRIGHTNESS = 60;
constexpr uint8_t DISPLAY_BRIGHTNESS = 5; // TM1637, 0..7.
constexpr uint16_t DISPLAY_RETRY_MS = 100;
constexpr uint16_t LED_RESET_US = 300;
constexpr uint8_t TM_BIT_US = 10;
constexpr bool DEBUG_SOLUTION = false;

static_assert(FLASH_ON_MS && FLASH_OFF_MS && SHOW_PAUSE_MS && STAGE_DONE_MS,
              "Game intervals must be positive");
static_assert(STAGE_BLINK_MS && DEBOUNCE_MS && DISPLAY_RETRY_MS && TM_BIT_US,
              "Display and button intervals must be positive");
static_assert(DISPLAY_BRIGHTNESS <= 7, "TM1637 brightness must be 0..7");

} // namespace Config
} // namespace Simon
