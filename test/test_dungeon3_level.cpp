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
TEST(d3_has_ice_gate_and_fire_obstacle){
  bool ice_cleared=false, fire_obstacle=false;
  for(int i=0;i<DUNGEON3_DATA.gate_count;++i){
    GateType t = DUNGEON3_DATA.gates[i].type;
    if(gate_cleared_by(t)==SpellId::Ice) ice_cleared=true;            // fire-wall (or water) gate
    if(t==GateType::Vine || t==GateType::Ice) fire_obstacle=true;     // Fire-cleared
  }
  CHECK(ice_cleared);                // an Ice-cleared gate exists
  CHECK(fire_obstacle); }            // proves dual-spell (Fire still needed)
TEST(d3_cage_exit_enemy){
  CHECK(DUNGEON3_DATA.has_cage); CHECK(DUNGEON3_DATA.has_exit);
  CHECK(DUNGEON3_DATA.enemy_count >= 1); }

// M8 retrofit: hub-return door (Q) near the spawn — exactly one, grounded on the main floor.
static int d3_tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(d3_has_hub_return_door){
  const LevelData& L = DUNGEON3_DATA;
  int hub_doors = 0;
  for(int i = 0; i < L.room_door_count; ++i)
    if(L.room_doors[i].target_room == -1) ++hub_doors;
  CHECK_EQ(hub_doors, 1);
}
TEST(d3_hub_door_grounds_on_main_floor){
  // The Q door's 2-wide archway (cols tx, tx+1) must ground on the main bottom floor (row h-2).
  const LevelData& L = DUNGEON3_DATA;
  const int floor_row = L.h - 2;   // row 20 for h=22
  for(int i = 0; i < L.room_door_count; ++i){
    if(L.room_doors[i].target_room != -1) continue;
    for(int dx = 0; dx < 2; ++dx){
      int col = L.room_doors[i].tx + dx;
      int fr = -1;
      for(int y = L.room_doors[i].ty + 1; y < L.h; ++y)
        if(d3_tile(L, col, y) == 1){ fr = y; break; }
      CHECK_EQ(fr, floor_row);
    }
  }
}
