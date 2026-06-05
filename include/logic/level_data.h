#pragma once
#include <cstdint>
#include "logic/gates.h"
namespace logic {
struct EntitySpawn { int tx, ty, param0, param1; };   // enemy: param0/1 = patrol left/right tile
struct GateSpawn   { int tx, ty; GateType type; };
struct DoorSpawn   { int tx, ty, dungeon; };          // dungeon 1..8
struct LevelData {
    const uint8_t* tiles; int w, h;
    int spawn_tx, spawn_ty;
    bool has_cage; int cage_tx, cage_ty;
    bool has_exit; int exit_tx, exit_ty;
    const EntitySpawn* enemies; int enemy_count;
    const GateSpawn*   gates;   int gate_count;
    const DoorSpawn*   doors;   int door_count;
};
}
