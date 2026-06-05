#pragma once
#include "logic/world_state.h"
namespace game {
struct HubResult { int enter_dungeon; }; // 1..8 = enter that dungeon
// Runs the plaza until the player enters a door (only Dungeon 1 is enterable in M2).
// Gate/door state is rendered from `world` on entry: a passable gate is opened.
HubResult run_hub(logic::World& world);
}
