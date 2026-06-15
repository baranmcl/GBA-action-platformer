#include "test_framework.h"
#include "game/levels/dungeon4.h"
using namespace logic;
TEST(d4_is_vertical){ CHECK(DUNGEON4_DATA.h > 22); CHECK(DUNGEON4_DATA.h <= 128); }  // tall climb (big-map bg cap)
TEST(d4_border_solid){
  const auto& L=DUNGEON4_DATA;
  for(int x=0;x<L.w;++x){ CHECK_EQ((int)L.tiles[x],1); CHECK_EQ((int)L.tiles[(L.h-1)*L.w+x],1); } }
TEST(d4_glide_shrine){
  CHECK_EQ(DUNGEON4_DATA.pickup_count, 1);
  CHECK(DUNGEON4_DATA.pickups[0].ability == Ability::Glide); }
TEST(d4_has_updraft_and_wind){
  bool up=false, wind=false;
  for(int i=0;i<DUNGEON4_DATA.w*DUNGEON4_DATA.h;++i){
    uint8_t t=DUNGEON4_DATA.tiles[i];
    if(t==(uint8_t)TileKind::Updraft) up=true;
    if(t==(uint8_t)TileKind::WindLeft || t==(uint8_t)TileKind::WindRight) wind=true;
  }
  CHECK(up); CHECK(wind); }
TEST(d4_has_vine_gate){
  bool vine=false;
  for(int i=0;i<DUNGEON4_DATA.gate_count;++i) if(DUNGEON4_DATA.gates[i].type==GateType::Vine) vine=true;
  CHECK(vine); }                       // the Fire beat
TEST(d4_cage_exit_vertical){
  CHECK(DUNGEON4_DATA.has_cage); CHECK(DUNGEON4_DATA.has_exit);
  CHECK(DUNGEON4_DATA.spawn_ty > DUNGEON4_DATA.exit_ty); }   // spawn below exit (ascent)

// M8 retrofit: hub-return door (Q) near the spawn — exactly one, grounded on the main bottom floor.
static int d4_tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(d4_has_hub_return_door){
  const LevelData& L = DUNGEON4_DATA;
  int hub_doors = 0;
  for(int i = 0; i < L.room_door_count; ++i)
    if(L.room_doors[i].target_room == -1) ++hub_doors;
  CHECK_EQ(hub_doors, 1);
}
TEST(d4_hub_door_grounds_on_main_floor){
  // The Q door's 2-wide archway (cols tx, tx+1) must ground on the main bottom floor (row h-2).
  // For D4 (h=56), floor_row = 54 (the solid floor the player starts on near the spawn).
  const LevelData& L = DUNGEON4_DATA;
  const int floor_row = L.h - 2;   // row 54 for h=56
  for(int i = 0; i < L.room_door_count; ++i){
    if(L.room_doors[i].target_room != -1) continue;
    for(int dx = 0; dx < 2; ++dx){
      int col = L.room_doors[i].tx + dx;
      int fr = -1;
      for(int y = L.room_doors[i].ty + 1; y < L.h; ++y)
        if(d4_tile(L, col, y) == 1){ fr = y; break; }
      CHECK_EQ(fr, floor_row);
    }
  }
}
