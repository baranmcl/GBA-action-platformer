#pragma once
#include "logic/vec2.h"
namespace logic {
struct BoltSpawn { Vec2 pos; Vec2 vel; };
// The wand's free magic bolt: no magic cost, only a per-cast cooldown (in ticks).
struct BoltCaster {
    int cooldown_ticks = 12;
    int cd = 0;
    void tick(){ if(cd > 0) --cd; }
    bool try_fire(bool pressed, Vec2 origin, int facing, BoltSpawn& out){
        if(!pressed || cd > 0) return false;
        out.pos = origin;
        out.vel = Vec2{ Fixed::from_int(3 * facing), Fixed::from_int(0) };
        cd = cooldown_ticks;
        return true;
    }
};
}
