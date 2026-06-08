#pragma once
#include "logic/collision.h"
namespace logic {
struct InputFrame { bool left=false, right=false, jump_pressed=false, fire_pressed=false, glide_held=false; };
struct Abilities { bool featherleap=false; bool glide=false; };
struct Player {
    Body body;
    Abilities abilities;
    int facing = 1;            // 1 right, -1 left
    int air_jumps_left = 0;    // refreshed to 1 on landing if featherleap
    void update(const InputFrame& in, const Tilemap& map);
};
}
