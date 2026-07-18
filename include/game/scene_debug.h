#pragma once
#include "logic/world_state.h"
namespace game {
// Result of the title-screen debug selector (I35). `dungeon` is 1..9 to launch (9 = the finale
// approach+arena, same as door 9 in the hub), or 0 if the player backed out (B) without picking
// one. `world` is a synthetic, session-only World built from the menu's toggles/counters.
//
// SAFETY: `world` must NEVER be persisted. It is never written by this scene, but normal gameplay
// (run_dungeon/run_boss) auto-saves on its own (deaths, latches, pickups, spronks) via
// engine::write_world all over src/game — the caller MUST call engine::set_save_enabled(false)
// before running gameplay with this World, or the debug session's fake progress silently
// overwrites the player's real SRAM save.
struct DebugLaunch { logic::World world; int dungeon; };

// Dev tool entered from the title screen by holding SELECT while pressing START (see
// scene_title.cpp). Lets a developer pick a dungeon (1-9), toggle each ability, and grant
// spronks directly, so late-game content can be QA'd without a full playthrough. Returns when
// START launches (or B cancels, dungeon == 0). Not polished UI — see scene_debug.cpp for the
// on-screen control legend.
DebugLaunch run_debug_select();
}
