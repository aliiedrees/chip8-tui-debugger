#include "Chip8.h"

// c'tor
Chip8::Chip8()
    : pc(0x200),
      I(0),
      sp(0),
      dt(0),
      st(0),
      memory{},
      stack{},
      V{},
      display{},
      keyboard{}
{
}