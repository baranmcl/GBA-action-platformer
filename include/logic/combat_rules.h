#pragma once
#include "logic/meters.h"
namespace logic {
inline constexpr int CONTACT_DAMAGE    = 20;
inline constexpr int HIT_IFRAMES       = 45;   // re-arm window after any hit
inline constexpr int KILL_MAGIC_REFILL = 25;
inline constexpr int RESPAWN_IFRAMES   = 60;   // post-respawn grace
static_assert(RESPAWN_IFRAMES > HIT_IFRAMES, "grace must exceed re-arm or hazard spawns death-loop");
// One contact/hazard hit attempt, exactly as every scene runs it. Returns true if damage landed.
inline bool try_hit(Meter& health, int& invuln, bool dash_invincible, bool overlapping){
    if(invuln == 0 && !dash_invincible && overlapping){ health.damage(CONTACT_DAMAGE); invuln = HIT_IFRAMES; return true; }
    return false;
}
inline void tick_iframes(int& invuln){ if(invuln > 0) --invuln; }
inline void respawn_vitals(Meter& health, int& invuln){ health.cur = health.max; invuln = RESPAWN_IFRAMES; }
}
