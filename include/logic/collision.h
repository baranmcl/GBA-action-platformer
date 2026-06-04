#pragma once
#include "logic/physics.h"
#include "logic/tilemap.h"
namespace logic {
void move_and_collide(Body& b, const Tilemap& map);
// Shared AABB overlap (top-left + half-extents). Reused later by enemy + cage pickup.
bool aabb_overlap(const Body& a, const Body& b);
}
