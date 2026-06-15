#pragma once
#include "logic/bolt.h"        // BoltSpawn
#include "logic/meters.h"      // Meter
#include "logic/world_state.h" // World, Ability
namespace logic {
enum class SpellId : uint8_t { None=0, Fire, Ice, Grapple, Light };  // Grapple: free traversal tool; Light: M10 reveal spell

// Spell-agnostic caster: spends magic + yields a projectile. The POOL tags the projectile with
// SpellState::selected (Fire vs Ice); the caster itself doesn't care which spell it is.
struct SpellCast {
    int cost = 10;   // cheap enough that a few enemy kills fund a dungeon's spell obstacles
    int cooldown_ticks = 20;
    int cd = 0;
    void tick(){ if(cd>0) --cd; }
    bool try_cast(bool pressed, Meter& magic, Vec2 origin, int facing, BoltSpawn& out){
        if(!pressed || cd>0) return false;
        if(!magic.spend(cost)) return false;
        out.pos = origin;
        out.vel = Vec2{ Fixed::from_int(2*facing), Fixed::from_int(0) };
        cd = cooldown_ticks;
        return true;
    }
};

// Selected-spell + availability. refresh() picks the first owned spell; cycle() rotates owned spells.
// NOTE: SpellId::Grapple is a non-magic traversal tool sharing the L-cycle/R-fire UI; the scene branches on selected==Grapple rather than firing a projectile (SpellCast never sees it).
struct SpellState {
    SpellId selected = SpellId::None;
    bool owns(const World& w, SpellId s) const {
        return (s==SpellId::Fire    && w.has(Ability::Fire))    ||
               (s==SpellId::Ice     && w.has(Ability::Ice))     ||
               (s==SpellId::Grapple && w.has(Ability::Grapple)) ||
               (s==SpellId::Light   && w.has(Ability::Light));
    }
    bool has_any(const World& w) const { return owns(w,SpellId::Fire) || owns(w,SpellId::Ice) || owns(w,SpellId::Grapple) || owns(w,SpellId::Light); }
    void refresh(const World& w){                    // first owned: Fire, then Ice, then Grapple (Fire/Ice preferred); Light last
        if(owns(w,SpellId::Fire))         selected = SpellId::Fire;
        else if(owns(w,SpellId::Ice))     selected = SpellId::Ice;
        else if(owns(w,SpellId::Grapple)) selected = SpellId::Grapple;
        else if(owns(w,SpellId::Light))   selected = SpellId::Light;
        else                              selected = SpellId::None;
    }
    void cycle(const World& w){                      // rotate Fire->Ice->Grapple->Light among owned spells
        if(!has_any(w)){ selected = SpellId::None; return; }
        SpellId order[4] = { SpellId::Fire, SpellId::Ice, SpellId::Grapple, SpellId::Light };
        int start = (selected==SpellId::Ice) ? 1 : (selected==SpellId::Grapple) ? 2 : (selected==SpellId::Light) ? 3 : 0;
        for(int i=1;i<=4;++i){ SpellId c = order[(start+i)%4]; if(owns(w,c)){ selected = c; return; } }
    }
    // Initialize a default selection WITHOUT clobbering a valid one. Call on scene entry and
    // after granting an ability: if the current `selected` is None or not owned, fall back to
    // refresh() (first owned); otherwise LEAVE the player's cycled choice untouched. This is what
    // makes the selection persist across rooms/hub/scenes and survive a shrine pickup (grabbing a
    // new ability auto-selects it ONLY if you had nothing valid, never overriding your choice).
    void ensure_valid(const World& w){
        if(selected == SpellId::None || !owns(w, selected)) refresh(w);
    }
};
}
