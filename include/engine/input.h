#pragma once
#include "logic/player.h"
namespace engine {
// Reads the GBA keypad into a pure logic::InputFrame for player.update().
// Controls: D-pad = move, A = jump, B = fire.
logic::InputFrame read_input();
}
