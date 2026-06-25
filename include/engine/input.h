#pragma once
#include "logic/player.h"
namespace engine {
// Reads the GBA keypad into a pure logic::InputFrame for player.update().
// Controls: D-pad = move, A = jump, B = fire.
logic::InputFrame read_input();

// Shot-aim height offset (px) added to the muzzle Y when firing (Zelda II-style high/mid/low):
// hold UP = fire HIGH (negative), DOWN = fire LOW (positive, kept small so the shot clears the
// floor), else MEDIUM (0). Shared by every combat scene so aiming is universal.
int read_aim_dy();
}
