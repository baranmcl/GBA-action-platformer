#pragma once
#include "logic/collision.h"
namespace logic {
// A simple ground patroller: walks between x bounds, reverses at a bound or a wall,
// falls under gravity, and dies when hit. Pure logic (host-testable).
struct Enemy {
    Body body;
    int dir = 1;                        // +1 right, -1 left
    Fixed left_bound{}, right_bound{};  // patrol range for body.pos.x (pixels)
    bool alive = true;
    bool fire_immune = false;           // M3: survives Fire, must be killed with the bolt
    void update(const Tilemap& map);
    void kill(){ alive = false; }
};
}
