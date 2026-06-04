#pragma once
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/world_state.h" // World
namespace logic {
// Freeing the spronk in Dungeon 1: when Laurel overlaps the cage, grant Featherleap
// and open the dungeon gate. Idempotent once freed. Returns true if freed (now or before).
inline bool try_free_spronk(const Body& player, const Body& cage, World& w){
    if(aabb_overlap(player, cage)){
        w.spronk1_freed = true;
        w.featherleap   = true;
        w.gate1_open     = true;
        return true;
    }
    return w.spronk1_freed;
}
}
