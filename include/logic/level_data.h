#pragma once
#include <cstdint>
#include "logic/gates.h"
#include "logic/ability_pickup.h"   // AbilityPickup { tx, ty, ability }
namespace logic {
struct EntitySpawn { int tx, ty, param0, param1, param2; }; // enemy: param0/1 patrol l/r tile; param2 flags (bit0 = fire_immune)
struct GateSpawn   { int tx, ty; GateType type; int latch_id = -1; }; // latch_id -1 = not latched
struct DoorSpawn   { int tx, ty, dungeon; };                // dungeon 1..8
struct BlockSpawn  { int tx, ty; bool pullable = false; };
struct PlateSpawn  { int tx, ty, target_tx, target_ty; };
struct ButtonSpawn { int tx, ty, target_tx, target_ty; };
struct BrazierSpawn{ int tx, ty, group; };
struct BrazierGroupSpawn { int total, target_tx, target_ty; int latch_id = -1; }; // latch_id -1 = not latched; a group of braziers shares one target
struct EntranceSpawn { int id, tx, ty, facing; };     // facing: -1 left, +1 right. id 0 = default/start.
struct RoomDoorSpawn { int tx, ty, target_room, target_entrance; }; // room-graph door: leads to another room in the same dungeon (not the hub→dungeon DoorSpawn)
struct HeartContainerSpawn { int tx, ty, id; };       // permanent HP upgrade pickup; id maps to World::heart_container_collected(id)
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
    const EntranceSpawn* entrances  = nullptr; int entrance_count  = 0;
    const RoomDoorSpawn* room_doors = nullptr; int room_door_count = 0;
    const HeartContainerSpawn* heart_containers = nullptr; int heart_container_count = 0;
};
struct DungeonData {
    const LevelData* const* rooms;   // rooms[0..room_count-1]
    int room_count;
    int start_room;
};
}
