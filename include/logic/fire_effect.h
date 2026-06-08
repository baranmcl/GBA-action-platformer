#pragma once
#include "logic/gates.h"
namespace logic {
// A fire projectile clears a gate iff the gate's required ability is Fire (vine, ice).
inline bool fire_clears_gate(GateType t){ return gate_info(t).required == Ability::Fire; }
}
