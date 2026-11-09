#pragma once

#include "module_bus.h"

namespace Simon {

enum class Action : uint8_t { None, NewGame, Halt };

class Node {
public:
    void begin();
    Action update();
    void smallError();
    void solved();
    const ModuleBus::Frame &status() const { return status_; }

private:
    void publish();
    ModuleBus::Frame status_;
};

} // namespace Simon
