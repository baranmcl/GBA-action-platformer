#pragma once
#include "logic/bolt.h"        // BoltSpawn
#include "logic/meters.h"      // Meter
#include "logic/world_state.h" // World, Ability
namespace logic {
enum class SpellId : uint8_t { None=0, Fire };   // D3+: Ice, Light, Stone...

struct FireCast {
    int cost = 25;
    int cooldown_ticks = 20;
    int cd = 0;
    void tick(){ if(cd>0) --cd; }
    // Spends magic + yields a fire projectile (slower than the free bolt's 3 px/tick).
    bool try_cast(bool pressed, Meter& magic, Vec2 origin, int facing, BoltSpawn& out){
        if(!pressed || cd>0) return false;
        if(!magic.spend(cost)) return false;
        out.pos = origin;
        out.vel = Vec2{ Fixed::from_int(2*facing), Fixed::from_int(0) };
        cd = cooldown_ticks;
        return true;
    }
};

// Selected-spell + availability (M3: only Fire). refresh() picks the first owned spell.
struct SpellState {
    SpellId selected = SpellId::None;
    bool has_any(const World& w) const { return w.has(Ability::Fire); }
    void refresh(const World& w){ selected = w.has(Ability::Fire) ? SpellId::Fire : SpellId::None; }
    void cycle(const World& w){ refresh(w); } // M3: one spell, cycle is a no-op (wiring for D3+)
};
}
