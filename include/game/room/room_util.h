#pragma once
#include "logic/fixed.h"
#include "logic/collision.h"   // Body
#include "logic/tilemap.h"     // Tilemap
namespace game {

// Small helpers shared by the room-system units (Task 6.1 of the maintainability
// remediation). Copied verbatim from the identical anonymous-namespace helpers that
// already exist per-.cpp across this game layer (scene_dungeon.cpp, player_session.cpp,
// boss_fight.cpp, scene_boss.cpp, scene_hub.cpp) — same pattern, just made available to the
// new src/game/room/*.cpp units too. scene_dungeon.cpp keeps its OWN local copies (still
// used there by the NOT-yet-moved brazier family), so this header does not replace them.
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
}
