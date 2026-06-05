#include "test_framework.h"
#include "logic/level_data.h"
using namespace logic;
static const uint8_t TILES[] = { 1,1,1, 1,0,1, 1,1,1 };
static const EntitySpawn ENEMIES[] = { {1,1,0,0} };
static const GateSpawn GATES[] = { {2,2,GateType::Gap} };
static const DoorSpawn DOORS[] = { {1,1,2} };
static constexpr LevelData L = { TILES,3,3, 1,1, true,1,1, true,2,2, ENEMIES,1, GATES,1, DOORS,1 };
TEST(level_dims){ CHECK_EQ(L.w,3); CHECK_EQ(L.h,3); }
TEST(level_spawn){ CHECK_EQ(L.spawn_tx,1); CHECK_EQ(L.spawn_ty,1); }
TEST(level_counts){ CHECK_EQ(L.enemy_count,1); CHECK_EQ(L.gate_count,1); CHECK_EQ(L.door_count,1); }
TEST(level_tile_at){ CHECK_EQ((int)L.tiles[L.spawn_ty*L.w + L.spawn_tx], 0); } // spawn on empty
TEST(level_door_dungeon){ CHECK_EQ(L.doors[0].dungeon, 2); }
