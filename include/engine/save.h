#pragma once
#include "logic/world_state.h"
namespace engine {
// Persist / restore progression to cartridge SRAM (battery-backed).
void write_world(const logic::World& w);
bool read_world(logic::World& out); // false if SRAM is empty/corrupt (fresh game)
}
