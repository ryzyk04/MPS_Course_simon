#include "drivers/Board.h"

#include <avr/io.h>

namespace Simon {
namespace Hardware {

void prepareTwi()
{
    DDRC &= uint8_t(~(_BV(PC4) | _BV(PC5)));
    PORTC |= _BV(PC4) | _BV(PC5);
}

} // namespace Hardware
} // namespace Simon
