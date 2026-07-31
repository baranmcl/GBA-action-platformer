#pragma once
#include <cstdint>
#include "logic/level_data.h"
#include "logic/tilemap.h"
#include "engine/level_view.h"
namespace engine {
// A live level: the collision Tilemap (over a MUTABLE shared buffer, so gates can flip
// tiles at runtime) plus the Butano background. Built from compiled LevelData.
struct LoadedLevel {
    logic::Tilemap map;   // .cells points at the mutable s_grid below
    LevelView view;       // the bg
    int generation;       // engine::level_generation() at load time; see set_collision_tile below
};
LoadedLevel load_level(const logic::LevelData& level);

// Flip a collision tile in the shared mutable grid (pair with engine::set_level_tile for visuals).
// SINGLE-LEVEL CONTRACT: there is exactly one shared s_grid; writes always target whichever
// level's load_level() call ran most recently, regardless of which LoadedLevel the caller holds.
// A stale LoadedLevel from an earlier load_level() will silently observe/mutate the wrong grid.
// `generation` is a seam for a future assert (not checked yet) once two levels can coexist.
void set_collision_tile(int tx, int ty, uint8_t v);

// Monotonically increasing counter bumped once per load_level() call. Compare a LoadedLevel's
// `generation` against this to detect a stale handle (currently unused — no caller asserts on it).
int level_generation();
}
