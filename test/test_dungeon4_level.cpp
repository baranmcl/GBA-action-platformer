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
