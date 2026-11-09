#pragma once

#include <PuzzleBus/AvrTwi.h>

namespace Simon {

enum class Action : uint8_t { None, NewGame, Halt };

// Тонкая связь игровой модели с общей библиотекой.
// Только библиотека владеет TWI_vect и кодирует пакеты.
class Node {
public:
    Node() : node_(PuzzleBus::PuzzleType::Simon), bus_(node_) {}

    bool begin();
    Action update();
    void ready();
    void smallError();
    void solved();
    void sync();

    const PuzzleBus::Status &status() const { return node_.status(); }

private:
    PuzzleBus::PuzzleNode node_;
    PuzzleBus::AvrTwiNode bus_;
};

} // namespace Simon
