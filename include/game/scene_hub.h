#pragma once
#include "logic/world_state.h"
#include "logic/player_state.h"
namespace game {
struct HubResult { int enter_dungeon; }; // 1..8 = enter that dungeon
// Runs the plaza until the player enters a door (Dungeon 1, plus Dungeon 2 once D1 is cleared).
// Gate/door state is rendered from `world` on entry: a passable gate is opened.
// `ps` is shown on the HUD so persistent health/magic are visible in the hub too.
HubResult run_hub(logic::World& world, logic::PlayerState& ps);
}
