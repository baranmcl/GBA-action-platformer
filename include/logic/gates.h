#pragma once
#include <cstdint>
#include "logic/world_state.h" // Ability
namespace logic {
enum class GateType : uint8_t {
    Gap=0,        // geometry: double-jump (Featherleap)
    GrapplePoint, // geometry: Grapple
    Vine,         // obstacle: Fire
    Ice,          // obstacle: Fire (melts)
    Water,        // obstacle: Ice (freezes to platform)
    CrackedWall,  // obstacle: Dash
    CrackedFloor, // obstacle: Stone (ground-pound)
    DarkVeil,     // obstacle: Light
    Count
};
struct GateInfo { Ability required; bool is_geometry; uint8_t bg_tile; }; // bg_tile = the CLOSED-state visual tile index (a wall the player sees while gated)
constexpr GateInfo GATE_TABLE[(int)GateType::Count] = {
    /*Gap*/          { Ability::Featherleap, true,  3 },  // closed = gate tile (3); only one instantiated in M2
    /*GrapplePoint*/ { Ability::Grapple,     true,  3 },
    /*Vine*/         { Ability::Fire,         false, 7 },  // tiles 7-12 are M3+ obstacle-gate art (not in tiles.bmp yet)
    /*Ice*/          { Ability::Fire,         false, 8 },
    /*Water*/        { Ability::Ice,          false, 9 },
    /*CrackedWall*/  { Ability::Dash,         false, 10 },
    /*CrackedFloor*/ { Ability::Stone,        false, 11 },
    /*DarkVeil*/     { Ability::Light,        false, 12 },
};
// Tile-index map (graphics/tiles.bmp): 0 blank, 1 ground, 2 one-way, 3 gate(closed wall),
// 4 cage, 5 door-open, 6 door-locked, 7-12 reserved for M3+ obstacle-gate art.
inline const GateInfo& gate_info(GateType t){ return GATE_TABLE[(int)t]; }
inline bool can_pass(GateType t, uint16_t abilities){ return (abilities >> (int)gate_info(t).required) & 1u; }
}
