#pragma once
#include <cstdint>
#include "logic/gates.h"
#include "logic/ability_pickup.h"   // AbilityPickup { tx, ty, ability }
namespace logic {
struct EntitySpawn { int tx, ty, param0, param1, param2; }; // enemy: param0/1 patrol l/r tile; param2 flags (bit0 = fire_immune)
struct GateSpawn   { int tx, ty; GateType type; };
struct DoorSpawn   { int tx, ty, dungeon; };                // dungeon 1..8
struct BlockSpawn  { int tx, ty; };
struct PlateSpawn  { int tx, ty, target_tx, target_ty; };
struct ButtonSpawn { int tx, ty, target_tx, target_ty; };
struct BrazierSpawn{ int tx, ty, group; };
struct BrazierGroupSpawn { int total, target_tx, target_ty; }; // a group of braziers shares one target
struct LevelData {
    const uint8_t* tiles; int w, h;
    int spawn_tx, spawn_ty;
    bool has_cage; int cage_tx, cage_ty;
    bool has_exit; int exit_tx, exit_ty;
    const EntitySpawn* enemies; int enemy_count;
    const GateSpawn*   gates;   int gate_count;
    const DoorSpawn*   doors;   int door_count;
    const AbilityPickup* pickups; int pickup_count;
    const BlockSpawn*  blocks;  int block_count;
    const PlateSpawn*  plates;  int plate_count;
    const ButtonSpawn* buttons; int button_count;
    const BrazierSpawn* braziers; int brazier_count;
    const BrazierGroupSpawn* brazier_groups; int brazier_group_count;
};
}
