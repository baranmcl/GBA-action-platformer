#pragma once
#include "logic/collision.h"
#include "logic/dash.h"
#include "logic/grapple.h"
namespace logic {
struct InputFrame { bool left=false, right=false, jump_pressed=false, fire_pressed=false, glide_held=false, grapple_fire=false; };
struct Abilities { bool featherleap=false; bool glide=false; bool dash=false; bool grapple=false; };
struct Player {
    Body body;
    Abilities abilities;
    DashState dash;            // M6 Blink: double-tap horizontal i-frame burst
    GrappleState grapple;      // M7 Vine Grapple: latch + pull toward anchor
    int facing = 1;            // 1 right, -1 left
    int air_jumps_left = 0;    // refreshed to 1 on landing if featherleap
    void update(const InputFrame& in, const Tilemap& map);
};
}
