#pragma once
#include "logic/meters.h"
#include "logic/spell.h"
namespace logic {
// Runtime player vitals that persist across scenes within a session (hub <-> dungeons).
// Owned by main() and passed by reference into every scene, so health drained / magic
// banked in one room carries into the next. NOT saved to SRAM: a fresh boot starts here.
struct PlayerState {
    Meter health{ 100, 100 };
    Meter magic{ 0, 100 };
    // Selected spell/tool. Lives here (not per-scene) so the player's choice persists across
    // room transitions, the hub, AND hub<->dungeon. Scenes call spell.ensure_valid(world) on
    // entry to initialize a default without clobbering an already-valid cycled selection.
    SpellState spell;
    // The dungeon the player most recently ENTERED (1..8), or 0 if they've never left the title.
    // Set at the top of run_dungeon; read by run_hub to spawn the player at the door they just used
    // (so returning from dungeon N puts them at door N, not a fixed point). Persists in PlayerState
    // because world.current_dungeon is reset to 0 when a dungeon scene ends.
    int last_dungeon = 0;
};
}
