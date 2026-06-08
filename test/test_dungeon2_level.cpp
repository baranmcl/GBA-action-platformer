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
