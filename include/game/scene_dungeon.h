#pragma once
#include "logic/world_state.h"
#include "logic/level_data.h"
#include "logic/player_state.h"
namespace game {
enum class DungeonResult { Cleared, Quit, QuitToTitle, Restart /* internal-only: consumed inside run_dungeon, never returned */ };
// Play a dungeon described by `dungeon` (a table of rooms). Featherleap is owned per `world`.
// Freeing the spronk grants Featherleap (M2: Dungeon 1's reward) immediately so the double-jump
// exit is reachable. Returns only Cleared (reached the exit) or Quit (SELECT). START (Restart)
// is handled internally — it refills vitals and re-enters the current room as an anti-soft-lock
// reset; it is never returned to the caller.
// PRECONDITION: world.current_dungeon is the dungeon's number (1..8), set by the caller.
// `ps` carries health/magic in and out, so vitals persist across hub <-> dungeon transitions.
DungeonResult run_dungeon(const logic::DungeonData& dungeon, logic::World& world, logic::PlayerState& ps);
}
