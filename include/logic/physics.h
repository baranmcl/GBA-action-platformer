#pragma once
#include "logic/vec2.h"
namespace logic {
struct Body {
    // CONVENTION (do NOT change without updating collision + all body tests):
    //   pos = TOP-LEFT corner of the AABB, in pixels (fixed).
    //   half_w/half_h = HALF extents, so full size = (2*half_w, 2*half_h).
    Vec2 pos;
    Vec2 vel;      // px/tick (fixed)
    Fixed half_w, half_h;
    bool on_ground = false;
};
struct PhysicsParams {
    Fixed gravity;   // added to vel.y each tick
    Fixed terminal;  // max downward speed
};
inline void apply_gravity(Body& b, const PhysicsParams& p){
    b.vel.y = b.vel.y + p.gravity;
    if(b.vel.y > p.terminal) b.vel.y = p.terminal;
}
}
