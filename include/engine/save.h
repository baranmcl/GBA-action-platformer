#pragma once
#include "logic/world_state.h"
namespace engine {
// Persist / restore progression to cartridge SRAM (battery-backed).
void write_world(const logic::World& w);
bool read_world(logic::World& out); // false if SRAM is empty/corrupt (fresh game)

// I35 save gate: gameplay auto-saves constantly (deaths, latches, pickups, spronks, dungeon
// clears — write_world is called all over src/game). The title-screen debug selector launches
// gameplay with a synthetic, throwaway World; without this gate those auto-saves would silently
// overwrite the player's real SRAM progress. Default true (normal title->hub->dungeon flow is
// unaffected). The debug launch path disables saving before running the chosen dungeon/boss and
// re-enables it afterward. read_world is unaffected — reading is always safe.
void set_save_enabled(bool enabled);
}
