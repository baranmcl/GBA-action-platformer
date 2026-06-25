#pragma once
#include "logic/world_state.h"
#include "logic/player_state.h"
#include "logic/level_data.h"
namespace game {
// M11 Nightmare King boss fight. Drives the host-tested logic::BossState and renders the
// King + attacks + EXPOSED window in a fixed-screen arena. Has exactly TWO exits:
//   Victory      — King defeated.
//   QuitToTitle  — chosen at the Game-Over screen when lives hit 0.
// There is intentionally NO mid-fight quit-to-hub: once you enter, you win or game-over.
enum class BossResult { Victory, QuitToTitle };
BossResult run_boss(const logic::DungeonData& arena, logic::World& world, logic::PlayerState& ps);
}
