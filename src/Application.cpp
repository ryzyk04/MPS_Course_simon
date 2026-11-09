#include "Application.h"
#include "drivers/Board.h"

namespace Simon {

void Application::begin()
{
    node_.begin();
    buttons_.reset(Hardware::readButtons(), Hardware::millis());
}

void Application::update()
{
    const uint32_t now = Hardware::millis();
    const Action action = node_.update();

    if (action == Action::NewGame) {
        game_.start(node_.status().seed, Level(node_.status().difficulty), now);
        buttons_.reset(Hardware::readButtons(), now);
        loggedStage_ = 0xFF;
    } else if (action == Action::Halt) {
        game_.stop();
    }

    // Нажатия в межэтапной паузе поглощаются, а не переносятся на новый этап.
    const uint8_t presses = buttons_.update(Hardware::readButtons(), now);
    for (uint8_t i = 0; i < 5; ++i) {
        if (!(presses & (1U << i))) {
            continue;
        }
        const Event event = game_.press(i + 1, now);
        if (event == Event::SmallError) {
            node_.smallError();
        } else if (event == Event::Solved) {
            node_.solved();
        }
    }

    game_.update(now);
    if (game_.state() == State::Playing && game_.stage() != loggedStage_) {
        loggedStage_ = game_.stage();
        Hardware::debugStage(loggedStage_, game_.sequence().buttons, game_.sequence().length);
    }
    display_.update(game_, now);
}

} // namespace Simon
