#include "test_framework.h"
#include "logic/level_data.h"
using namespace logic;
static const uint8_t TILES[] = { 1,1,1, 1,0,1, 1,1,1 };
static const EntitySpawn ENEMIES[] = { {1,1,0,0,0} };       // +param2
static const GateSpawn GATES[] = { {2,2,GateType::Gap} };
static const DoorSpawn DOORS[] = { {1,1,2} };
static const AbilityPickup PICKUPS[] = { {1,1,Ability::Fire} };
static const BlockSpawn BLOCKS[] = { {1,1} };
static const PlateSpawn PLATES[] = { {1,1,2,2} };
static const ButtonSpawn BUTTONS[] = { {1,1,2,2} };
static const BrazierSpawn BRAZIERS[] = { {1,1,0} };
static const BrazierGroupSpawn BGROUPS[] = { {2,2,2} };
static constexpr LevelData L = { TILES,3,3, 1,1, true,1,1, true,2,2,
    ENEMIES,1, GATES,1, DOORS,1, PICKUPS,1, BLOCKS,1, PLATES,1, BUTTONS,1, BRAZIERS,1, BGROUPS,1 };
TEST(level_dims){ CHECK_EQ(L.w,3); CHECK_EQ(L.h,3); }
TEST(level_spawn){ CHECK_EQ(L.spawn_tx,1); CHECK_EQ(L.spawn_ty,1); }
TEST(level_counts){ CHECK_EQ(L.enemy_count,1); CHECK_EQ(L.gate_count,1); CHECK_EQ(L.door_count,1); }
TEST(level_m3_counts){ CHECK_EQ(L.pickup_count,1); CHECK_EQ(L.block_count,1); CHECK_EQ(L.plate_count,1);
  CHECK_EQ(L.button_count,1); CHECK_EQ(L.brazier_count,1); CHECK_EQ(L.brazier_group_count,1); }
TEST(level_tile_at){ CHECK_EQ((int)L.tiles[L.spawn_ty*L.w + L.spawn_tx], 0); } // spawn on empty
TEST(level_door_dungeon){ CHECK_EQ(L.doors[0].dungeon, 2); }
TEST(level_brazier_group_target){ CHECK_EQ(L.brazier_groups[0].total, 2); CHECK_EQ(L.brazier_groups[0].target_tx, 2); }

// --- Heart container spawn ---
static const HeartContainerSpawn HC_SPAWNS[] = { {3, 5, 2} };
static constexpr LevelData L_HC = { TILES,3,3, 1,1, false,0,0, false,0,0,
    ENEMIES,1, GATES,1, DOORS,1, PICKUPS,1, BLOCKS,1, PLATES,1, BUTTONS,1,
    BRAZIERS,1, BGROUPS,1, nullptr,0, nullptr,0,
    HC_SPAWNS, 1 };
TEST(heart_container_spawn_fields){
    CHECK_EQ(L_HC.heart_container_count, 1);
    CHECK_EQ(L_HC.heart_containers[0].tx, 3);
    CHECK_EQ(L_HC.heart_containers[0].ty, 5);
    CHECK_EQ(L_HC.heart_containers[0].id, 2);
}
TEST(heart_container_null_when_absent){
    CHECK_EQ(L.heart_container_count, 0);
    // Default-initialised pointer is nullptr — don't dereference, just check count.
}
