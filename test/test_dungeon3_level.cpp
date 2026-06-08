#include "test_framework.h"
#include "game/levels/dungeon3.h"
using namespace logic;
TEST(d3_dims){ CHECK_EQ(DUNGEON3_DATA.w, 64); CHECK_EQ(DUNGEON3_DATA.h, 22); }
TEST(d3_border_solid){
  const auto& L=DUNGEON3_DATA;
  for(int x=0;x<L.w;++x){ CHECK_EQ((int)L.tiles[x],1); CHECK_EQ((int)L.tiles[(L.h-1)*L.w+x],1); } }
TEST(d3_ice_shrine){
  CHECK_EQ(DUNGEON3_DATA.pickup_count, 1);
  CHECK(DUNGEON3_DATA.pickups[0].ability == Ability::Ice); }
TEST(d3_has_water_tiles){
  bool water=false;
  for(int i=0;i<DUNGEON3_DATA.w*DUNGEON3_DATA.h;++i) if(DUNGEON3_DATA.tiles[i]==(uint8_t)TileKind::Water) water=true;
  CHECK(water); }
TEST(d3_has_water_gate_and_fire_obstacle){
  bool water_gate=false, fire_obstacle=false;
  for(int i=0;i<DUNGEON3_DATA.gate_count;++i){
    if(DUNGEON3_DATA.gates[i].type==GateType::Water) water_gate=true;
    if(DUNGEON3_DATA.gates[i].type==GateType::Vine || DUNGEON3_DATA.gates[i].type==GateType::Ice) fire_obstacle=true;
  }
  CHECK(water_gate);                 // Ice-cleared
  CHECK(fire_obstacle); }            // proves dual-spell (Fire still needed)
TEST(d3_cage_exit_enemy){
  CHECK(DUNGEON3_DATA.has_cage); CHECK(DUNGEON3_DATA.has_exit);
  CHECK(DUNGEON3_DATA.enemy_count >= 1); }
