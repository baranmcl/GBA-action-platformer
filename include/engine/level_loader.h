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
};
LoadedLevel load_level(const logic::LevelData& level);

// Flip a collision tile in the shared mutable grid (pair with engine::set_level_tile for visuals).
void set_collision_tile(int tx, int ty, uint8_t v);
}
