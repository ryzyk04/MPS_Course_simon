#include "communication/Node.h"
#include "drivers/Board.h"

namespace Simon {

bool Node::begin()
{
    Hardware::prepareTwi();
    return bus_.begin(0x11);
}

Action Node::update()
{
    bus_.update();
    PuzzleBus::Command command;
    if (!node_.takeCommand(command)) {
        return Action::None;
    }

    return command.operation == PuzzleBus::Operation::Generate
        ? Action::NewGame : Action::Halt;
}

void Node::ready()
{
    node_.ready();
}

void Node::smallError()
{
    node_.smallError();
}

void Node::solved()
{
    node_.finish(true);
}

void Node::sync()
{
    bus_.update();
}

} // namespace Simon
