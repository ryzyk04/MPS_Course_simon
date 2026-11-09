#pragma once

#include "game/Puzzle.h"

namespace Simon {

enum class State : uint8_t { Waiting, Playing, StagePause, Solved };
enum class Event : uint8_t { None, SmallError, Solved };

class Game {
public:
    bool start(uint16_t seed, Level level, uint32_t now);
    void stop();
    void update(uint32_t now);
    Event press(uint8_t button, uint32_t now);

    State state() const { return state_; }
    uint16_t seed() const { return seed_; }
    Level level() const { return level_; }
    uint8_t stage() const { return stage_; }
    uint8_t inputPosition() const { return input_; }
    const Sequence &sequence() const { return sequence_; }
    uint8_t shownColor() const;
    bool errorActive() const { return error_; }

private:
    enum class Phase : uint8_t { On, Off, Pause };
    void startStage(uint32_t now);

    State state_ = State::Waiting;
    Phase phase_ = Phase::On;
    Level level_ = EASY;
    Random random_;
    Sequence sequence_;
    uint16_t seed_ = 0;
    uint8_t stage_ = 0;
    uint8_t input_ = 0;
    uint8_t shown_ = 0;
    uint32_t phaseAt_ = 0;
    uint32_t stageAt_ = 0;
    uint32_t errorAt_ = 0;
    bool error_ = false;
};

} // namespace Simon
