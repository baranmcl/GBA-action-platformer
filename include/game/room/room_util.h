#pragma once
#include "logic/fixed.h"
#include "logic/collision.h"    // Body
#include "logic/tilemap.h"      // Tilemap
#include "logic/world_state.h"  // World::latched/set_latch (persist_latch)
#include "engine/save.h"        // write_world (persist_latch) -- header is bn::-free (see below)
namespace game {

// Small helpers shared by the room-system units (Task 6.1 of the maintainability
// remediation). Copied verbatim from the identical anonymous-namespace helpers that
// already exist per-.cpp across this game layer (scene_dungeon.cpp, player_session.cpp,
// boss_fight.cpp, scene_boss.cpp, scene_hub.cpp) — same pattern, just made available to the
// new src/game/room/*.cpp units too. scene_dungeon.cpp no longer keeps its own copies as of
// Task 6.2 (the last two families that needed them -- braziers/gates -- moved out).
//
// Deliberately host-test-safe: every include above (and engine/save.h, added in Task 6.2 for
// persist_latch) declares only logic:: types in its own headers, so test/*.cpp may include this
// header with zero Butano dependency. fill_column/open_column are NOT here for that reason: they
// take an engine::LevelView&, and engine/level_view.h drags in bn_regular_bg_ptr.h -- they stay
// duplicated per-.cpp (gates_system.cpp, triggers_system.cpp) instead.
inline logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }

inline logic::Body tile_body(int tx, int ty, int hw, int hh){
    logic::Body b{}; b.half_w = fx(hw); b.half_h = fx(hh);
    b.pos = { fx(tx * 8), fx(ty * 8) }; return b;
}

// First STANDABLE collision row at/below start_ty in this column (the floor the content
// rests on). Standable = solid OR one-way platform. Falls back to start_ty+1 if none found.
inline int floor_row_below(const logic::Tilemap& map, int tx, int start_ty){
    for(int y = start_ty + 1; y < map.h; ++y)
        if(map.is_solid(tx, y) || map.is_oneway(tx, y)) return y;
    return start_ty + 1;
}

// Persist a progress latch to SRAM on first trigger; no-op if already set or unlatched (-1).
// Moved here from scene_dungeon.cpp's file-local helper (Task 6.2): both GatesSystem and
// TriggersSystem call it, so it lives in the shared header instead of being duplicated twice.
inline void persist_latch(logic::World& world, int latch_id){
    if(latch_id >= 0 && !world.latched(latch_id)){
        world.set_latch(latch_id);
        engine::write_world(world);
    }
}
}
