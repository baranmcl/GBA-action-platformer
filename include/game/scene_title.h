#pragma once
#include "logic/world_state.h"
namespace game {
// Shows the title; "CONTINUE" if a save exists. Returns when START is pressed.
void run_title(const logic::World& world);
}
