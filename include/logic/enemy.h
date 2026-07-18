#pragma once
#include "logic/collision.h"
namespace logic {

// Enemy-type seam (I10): today both values are the same ground patroller with a
// fire-immunity flag; the enum + switch below is the ONE place a future variant
// (flyer, shooter, ...) adds a row, instead of scattering param-bit checks.
enum class EnemyType : uint8_t { Patroller = 0, PatrollerFireImmune = 1 };

// Decodes EntitySpawn.param2 bit0 (the only bit currently used) into an EnemyType.
inline EnemyType enemy_type_from_params(int param2){
    return (param2 & 1) ? EnemyType::PatrollerFireImmune : EnemyType::Patroller;
}

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
