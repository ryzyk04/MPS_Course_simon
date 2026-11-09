#include "Application.h"
#include "drivers/Board.h"

#include <avr/interrupt.h>

namespace {
Simon::Application application;
}

int main()
{
    Simon::Hardware::beginBoard();
    Simon::Hardware::beginClock();
    application.begin();
    sei();

    for (;;) {
        application.update();
    }
}
