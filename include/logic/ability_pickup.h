#pragma once
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/world_state.h"
namespace logic {
struct AbilityPickup { int tx, ty; Ability ability; };
// Returns true ONLY on the frame the ability is newly taken (overlap + not already owned).
inline bool try_take_pickup(const Body& player, const Body& shrine, World& w, Ability a){
    if(aabb_overlap(player, shrine) && !w.has(a)){ w.grant(a); return true; }
    return false;
}
}
