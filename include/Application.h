#pragma once

#include "game/Game.h"
#include "game/Buttons.h"
#include "game/Display.h"
#include "communication/Node.h"

namespace Simon {

// Связывает независимую игровую модель с шиной и драйверами.
class Application {
public:
    void begin();
    void update();

private:
    Game game_;
    Buttons buttons_;
    Display display_;
    Node node_;
    uint8_t loggedStage_ = 0xFF;
};

} // namespace Simon
