#include "test_framework.h"
#include "game/levels/dungeon5.h"
using namespace logic;
TEST(d5_dims){ CHECK_EQ(DUNGEON5_DATA.w, 64); CHECK_EQ(DUNGEON5_DATA.h, 24); }  // w<=64, h<=128 (big-map limits)
TEST(d5_border_solid){
  const auto& L=DUNGEON5_DATA;
  for(int x=0;x<L.w;++x){ CHECK_EQ((int)L.tiles[x],1); CHECK_EQ((int)L.tiles[(L.h-1)*L.w+x],1); } }
TEST(d5_dash_shrine){
  CHECK_EQ(DUNGEON5_DATA.pickup_count, 1);
  CHECK(DUNGEON5_DATA.pickups[0].ability == Ability::Dash); }
TEST(d5_has_spike_tiles){
  bool spikes=false;
  for(int i=0;i<DUNGEON5_DATA.w*DUNGEON5_DATA.h;++i) if(DUNGEON5_DATA.tiles[i]==(uint8_t)TileKind::Spikes) spikes=true;
  CHECK(spikes); }
TEST(d5_has_cracked_wall){
  bool cracked=false;
  for(int i=0;i<DUNGEON5_DATA.gate_count;++i) if(DUNGEON5_DATA.gates[i].type==GateType::CrackedWall) cracked=true;
  CHECK(cracked); }
TEST(d5_combo_carried_power_obstacle){
  // Proves the combo theme: at least one carried-power beat besides the dash beats.
  bool gate_beat=false;
  for(int i=0;i<DUNGEON5_DATA.gate_count;++i){
    GateType t = DUNGEON5_DATA.gates[i].type;
    if(t==GateType::Vine || t==GateType::Ice || t==GateType::Water || t==GateType::FireWall) gate_beat=true;
  }
  bool wind_beat=false;
  for(int i=0;i<DUNGEON5_DATA.w*DUNGEON5_DATA.h;++i){
    uint8_t k = DUNGEON5_DATA.tiles[i];
    if(k==(uint8_t)TileKind::Updraft || k==(uint8_t)TileKind::WindLeft || k==(uint8_t)TileKind::WindRight) wind_beat=true;
  }
  CHECK(gate_beat);   // a Fire/Ice gate beat (Vine + Water in D5)
  CHECK(wind_beat); } // a Glide updraft beat
TEST(d5_cage_exit_enemy){
  CHECK(DUNGEON5_DATA.has_cage); CHECK(DUNGEON5_DATA.has_exit);
  CHECK(DUNGEON5_DATA.enemy_count >= 1); }
