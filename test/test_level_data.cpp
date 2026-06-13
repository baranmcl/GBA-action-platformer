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

// --- Task 1.4: M8 Stone spawn types ---
// PlateSpawn.heavy field
static const PlateSpawn HEAVY_PLATES[] = { {3, 5, 6, 5, /*heavy=*/true}, {1, 2, 4, 2} };
// BoulderSpawn
static const BoulderSpawn BOULDERS[] = { {7, 3} };
// LoosePlatformSpawn
static const LoosePlatformSpawn LOOSE[] = { {2, 4, 3} };
static constexpr LevelData L_M8 = { TILES,3,3, 1,1, false,0,0, false,0,0,
    ENEMIES,1, GATES,1, DOORS,1, PICKUPS,1, BLOCKS,1,
    HEAVY_PLATES,2, BUTTONS,1, BRAZIERS,1, BGROUPS,1,
    nullptr,0, nullptr,0, nullptr,0,
    BOULDERS,1, LOOSE,1 };
TEST(plate_heavy_field_default_false){
    // PlateSpawn.heavy defaults false
    PlateSpawn p{1,1,2,2};
    CHECK(!p.heavy);
}
TEST(plate_heavy_field_can_be_true){
    CHECK(HEAVY_PLATES[0].heavy);
    CHECK_EQ(HEAVY_PLATES[0].tx, 3); CHECK_EQ(HEAVY_PLATES[0].ty, 5);
}
TEST(plate_heavy_defaults_false_for_plain_plate){
    CHECK(!HEAVY_PLATES[1].heavy); // second plate: heavy not set -> false
}
TEST(boulder_spawn_fields){
    CHECK_EQ(L_M8.boulder_count, 1);
    CHECK_EQ(L_M8.boulders[0].tx, 7);
    CHECK_EQ(L_M8.boulders[0].ty, 3);
}
TEST(loose_platform_spawn_fields){
    CHECK_EQ(L_M8.loose_platform_count, 1);
    CHECK_EQ(L_M8.loose_platforms[0].tx, 2);
    CHECK_EQ(L_M8.loose_platforms[0].ty, 4);
    CHECK_EQ(L_M8.loose_platforms[0].len, 3);
}
TEST(boulder_null_when_absent){
    CHECK_EQ(L.boulder_count, 0); // existing L has no boulders -> count==0
}
TEST(loose_platform_null_when_absent){
    CHECK_EQ(L.loose_platform_count, 0); // existing L has no loose platforms -> count==0
}
