#pragma once
#include "logic/world_state.h"
#include "logic/level_data.h"
#include "logic/player_state.h"
namespace game {
enum class DungeonResult { Cleared, Quit, Restart };
// Play a dungeon described by `level`. Featherleap is owned per `world`. Freeing the
// spronk grants Featherleap (M2: Dungeon 1's reward) immediately so the double-jump exit
// is reachable. Returns Cleared on reaching the exit after the spronk is freed; Quit on SELECT;
// Restart on START (the caller re-runs the level fresh + refills vitals — an anti-soft-lock reset).
// PRECONDITION: world.current_dungeon is the dungeon's number (1..8), set by the caller.
// `ps` carries health/magic in and out, so vitals persist across hub <-> dungeon transitions.
DungeonResult run_dungeon(const logic::LevelData& level, logic::World& world, logic::PlayerState& ps);
}
