#include "test_framework.h"
#include "game/levels/dungeon2.h"
using namespace logic;
TEST(d2_dims){ CHECK_EQ(DUNGEON2_DATA.w, 64); CHECK_EQ(DUNGEON2_DATA.h, 22); }
TEST(d2_has_all_systems){
  CHECK(DUNGEON2_DATA.has_cage); CHECK(DUNGEON2_DATA.has_exit);
  CHECK_EQ(DUNGEON2_DATA.pickup_count, 1);     // Fire shrine
  CHECK_EQ(DUNGEON2_DATA.block_count, 1);
  CHECK_EQ(DUNGEON2_DATA.plate_count, 1);
  CHECK_EQ(DUNGEON2_DATA.button_count, 1);
  CHECK_EQ(DUNGEON2_DATA.brazier_count, 3);
  CHECK_EQ(DUNGEON2_DATA.brazier_group_count, 1);
  CHECK_EQ(DUNGEON2_DATA.gate_count, 2); }     // vine + ice
TEST(d2_fire_shrine){ CHECK(DUNGEON2_DATA.pickups[0].ability == Ability::Fire); }
TEST(d2_one_fire_immune_enemy){
  int immune = 0;
  for(int i=0;i<DUNGEON2_DATA.enemy_count;++i) if(DUNGEON2_DATA.enemies[i].param2 & 1) ++immune;
  CHECK_EQ(immune, 1); }
TEST(d2_lava_present){
  bool lava=false;
  for(int i=0;i<DUNGEON2_DATA.w*DUNGEON2_DATA.h;++i) if(DUNGEON2_DATA.tiles[i]==3) lava=true;
  CHECK(lava); }
TEST(d2_border_solid){
  const auto& L=DUNGEON2_DATA;
  for(int x=0;x<L.w;++x){ CHECK_EQ((int)L.tiles[x],1); CHECK_EQ((int)L.tiles[(L.h-1)*L.w+x],1); } }

// M8 retrofit: hub-return door (Q) near the spawn — exactly one, grounded on the main floor.
static int d2_tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(d2_has_hub_return_door){
  const LevelData& L = DUNGEON2_DATA;
  int hub_doors = 0;
  for(int i = 0; i < L.room_door_count; ++i)
    if(L.room_doors[i].target_room == -1) ++hub_doors;
  CHECK_EQ(hub_doors, 1);
}
TEST(d2_hub_door_grounds_on_main_floor){
  // The Q door's 2-wide archway (cols tx, tx+1) must ground on the main bottom floor (row h-2).
  const LevelData& L = DUNGEON2_DATA;
  const int floor_row = L.h - 2;   // row 20 for h=22
  for(int i = 0; i < L.room_door_count; ++i){
    if(L.room_doors[i].target_room != -1) continue;
    for(int dx = 0; dx < 2; ++dx){
      int col = L.room_doors[i].tx + dx;
      int fr = -1;
      for(int y = L.room_doors[i].ty + 1; y < L.h; ++y)
        if(d2_tile(L, col, y) == 1){ fr = y; break; }
      CHECK_EQ(fr, floor_row);
    }
  }
}
