#pragma once
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/world_state.h" // World
namespace logic {
// Overlap the cage -> free dungeon d's spronk. Idempotent. Returns true if freed (now or before).
// The ability grant (e.g. Featherleap) is the dungeon scene's job, not this helper's.
inline bool try_free_spronk(const Body& player, const Body& cage, World& w, int d){
    if(aabb_overlap(player, cage)){
        w.free_spronk(d);
        return true;
    }
    return w.spronk_freed(d);
}
}
