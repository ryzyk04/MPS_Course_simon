#include "communication/Node.h"
#include "drivers/Board.h"

namespace Simon {

void Node::begin()
{
    status_ = ModuleBus::Frame();
    status_.type = ModuleBus::SIMON;
    publish();
    Hardware::beginTwi(ModuleBus::SIMON_ADDRESS);
}

void Node::publish()
{
    uint8_t packet[ModuleBus::FRAME_SIZE];
    ModuleBus::encode(status_, packet);
    Hardware::publishStatus(packet);
}

Action Node::update()
{
    uint8_t packet[ModuleBus::FRAME_SIZE];
    if (!Hardware::takeCommand(packet)) {
        return Action::None;
    }

    ModuleBus::Frame command;
    if (!ModuleBus::decode(packet, sizeof(packet), command) ||
        command.type != ModuleBus::SIMON) {
        return Action::None;
    }

    if (command.operation == ModuleBus::GENERATE) {
        if (command.round == status_.round) {
            return Action::None;
        }

        status_.round = command.round;
        status_.seed = command.seed;
        status_.difficulty = command.difficulty;
        status_.errors = 0;
        status_.state = ModuleBus::PLAYING;
        publish();
        return Action::NewGame;
    }

    if (command.operation == ModuleBus::RESET ||
        (command.operation == ModuleBus::STOP && command.round == status_.round)) {
        status_.state = command.operation == ModuleBus::RESET
            ? ModuleBus::WAITING : ModuleBus::STOPPED;
        if (command.operation == ModuleBus::RESET) {
            status_.round = 0;
            status_.errors = 0;
        }
        publish();
        return Action::Halt;
    }

    return Action::None;
}

void Node::smallError()
{
    if (status_.state == ModuleBus::PLAYING && status_.errors != UINT16_MAX) {
        ++status_.errors;
        publish();
    }
}

void Node::solved()
{
    if (status_.state == ModuleBus::PLAYING) {
        status_.state = ModuleBus::SOLVED;
        publish();
    }
}

} // namespace Simon
