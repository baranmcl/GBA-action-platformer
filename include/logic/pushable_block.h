#pragma once
#include "logic/tilemap.h"
namespace logic {
// Grid-aligned pushable block. Logic only updates tx/ty; the SCENE keeps the block's
// tile marked Solid in the collision grid and syncs the sprite (see scene_dungeon).
struct PushableBlock {
    int tx, ty;
    bool active = true;
    // Slide one tile in dir (+1/-1) if the destination tile is non-solid; returns moved.
    bool push(int dir, const Tilemap& map){
        int nx = tx + dir;
        if(map.is_solid(nx, ty)) return false;   // wall/edge (OOB is solid)
        tx = nx; return true;
    }
    // Fall one tile if the tile below is non-solid; returns moved.
    bool apply_gravity_step(const Tilemap& map){
        if(map.is_solid(tx, ty+1)) return false;
        ++ty; return true;
    }
};
}
