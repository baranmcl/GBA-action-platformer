#pragma once
#include "logic/world_state.h"
namespace game {
// The finale "THE END" screen, shown after the Nightmare King is defeated.
// Fades in, shows the ending text, and returns when START is pressed.
void run_victory(const logic::World& world);
}
