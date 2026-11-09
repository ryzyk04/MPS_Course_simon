#include "game/Game.h"
#include "Config.h"

namespace Simon {

bool Game::start(uint16_t seed, Level level, uint32_t now)
{
    if (seed > 9999 || level > HARD) {
        return false;
    }

    seed_ = seed;
    level_ = level;
    random_ = Random(seed);
    stage_ = 0;
    error_ = false;
    startStage(now);
    return true;
}

void Game::stop()
{
    state_ = State::Waiting;
    error_ = false;
    input_ = 0;
}

void Game::startStage(uint32_t now)
{
    generateSequence(seed_, level_, stage_, random_, sequence_);
    state_ = State::Playing;
    phase_ = Phase::On;
    phaseAt_ = now;
    shown_ = 0;
    input_ = 0;
}

void Game::update(uint32_t now)
{
    if (error_ && uint32_t(now - errorAt_) >= Config::ERROR_MS) {
        error_ = false;
    }

    if (state_ == State::StagePause) {
        if (uint32_t(now - stageAt_) >= Config::STAGE_DONE_MS) {
            startStage(now);
        }
        return;
    }
    if (state_ != State::Playing) {
        return;
    }

    // За один проход меняем одну фазу, как в исходной прошивке.
    const uint32_t elapsed = now - phaseAt_;
    switch (phase_) {
    case Phase::On:
        if (elapsed >= Config::FLASH_ON_MS) {
            phase_ = Phase::Off;
            phaseAt_ = now;
        }
        break;
    case Phase::Off:
        if (elapsed >= Config::FLASH_OFF_MS) {
            phase_ = ++shown_ >= sequence_.length ? Phase::Pause : Phase::On;
            phaseAt_ = now;
        }
        break;
    case Phase::Pause:
        if (elapsed >= Config::SHOW_PAUSE_MS) {
            shown_ = 0;
            phase_ = Phase::On;
            phaseAt_ = now;
        }
        break;
    }
}

Event Game::press(uint8_t button, uint32_t now)
{
    if (state_ != State::Playing || button < 1 || button > 5) {
        return Event::None;
    }

    if (button != sequence_.buttons[input_]) {
        input_ = 0;
        error_ = true;
        errorAt_ = now;
        return Event::SmallError;
    }
    if (++input_ < sequence_.length) {
        return Event::None;
    }

    error_ = false;
    ++stage_;
    if (stage_ == STAGES) {
        state_ = State::Solved;
        return Event::Solved;
    }

    state_ = State::StagePause;
    stageAt_ = now;
    return Event::None;
}

uint8_t Game::shownColor() const
{
    return state_ == State::Playing && phase_ == Phase::On
        ? sequence_.colors[shown_] : NO_COLOR;
}

} // namespace Simon
