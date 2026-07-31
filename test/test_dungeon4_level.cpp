#include "test_framework.h"
#include "level_harness.h"
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

// ----------------------------------------------------------------------------
// Task 2.6 — real reachability via the shared harness (was existence-only coverage). The Vine gate
// (col 12) walls off the ENTIRE room past the spawn (build_grid fills it rows 1..h-3, i.e. nearly
// floor-to-ceiling for this h=56 room), so every proof below opens it (the "Fire beat" the Vine gate
// gates in this dungeon) -- otherwise nothing past the spawn is reachable at all.
// ----------------------------------------------------------------------------
static harness::WorldModel d4_open_gates_wm(){
  harness::WorldModel wm{};
  for(int i = 0; i < DUNGEON4_DATA.gate_count; ++i) wm.open_gates.insert(i);
  return wm;
}

TEST(d4_shrine_reachable_with_and_without_glide){
  // The Glide shrine sits BEFORE the updraft shaft (reached via the cols 12-18 ledge staircase,
  // each rung a 5-tile climb -- exactly CLIMB_RELIABLE), so it must be reachable with the ability
  // the player doesn't have yet (it's what grants glide) -- and, trivially, also reachable once
  // glide is held (glide only adds reach, never removes it).
  const LevelData& L = DUNGEON4_DATA;
  REQUIRE(L.pickup_count == 1);
  int tx = L.pickups[0].tx, ty = L.pickups[0].ty;
  harness::WorldModel no_glide = d4_open_gates_wm();
  CHECK(harness::reaches(L, no_glide, tx, ty));
  harness::WorldModel glide = d4_open_gates_wm(); glide.glide = true;
  CHECK(harness::reaches(L, glide, tx, ty));
}

// NEEDS_CONTEXT (discovered, not guessed): the row-35 ledge staircase (cols 12-18) and the updraft
// shaft's base (the row-37 OneWay cap, cols 25-28) sit on the SAME rows (33-37) but are separated by
// a 6-tile WindRight zone (cols 19-24) with no standable tile in between -- crossing it requires the
// sideways gust PUSH (src/logic/player.cpp: WIND_ACCEL, applied "regardless of ability"), which this
// harness deliberately does not model (Task 2.6 brief: "Do NOT model wind push force"). From spawn,
// harness::reaches(DUNGEON4_DATA, glide, exit/cage) is therefore NOT provable by this model -- it is
// not a soft-lock (the level almost certainly plays fine with real wind physics; the whole point of
// the WindRight strip lining up with the ledge row is clearly to blow the player across), just outside
// what a conservative climb-only model can certify. The test below proves the part that IS provable:
// seeded at the shaft's base (i.e. GIVEN the wind gust carries the player across, unmodeled), glide
// alone carries them the rest of the way to both the cage and the exit -- the updraft-lift rule is
// doing its job; the wind-push gap is the only unverified link. Report this gap in the task report
// rather than inventing a wind-push rule to force the spawn-seeded assertion to pass.
TEST(d4_glide_crosses_shaft_from_its_base){
  const LevelData& L = DUNGEON4_DATA;
  harness::WorldModel wm = d4_open_gates_wm(); wm.glide = true;
  CHECK(harness::reaches_from(L, wm, 26, 36, L.exit_tx, L.exit_ty));
  CHECK(harness::reaches_from(L, wm, 26, 36, L.cage_tx, L.cage_ty));
}

TEST(d4_exit_and_cage_require_glide){
  // MUST-NOT-bypass companion: without glide, the shaft stays unreachable even at the edge-apex
  // CLIMB_MAX jump -- the updraft is the only way up, not an unreliable pixel-perfect jump.
  const LevelData& L = DUNGEON4_DATA;
  harness::WorldModel wm = d4_open_gates_wm(); wm.climb_max = true;
  CHECK(!harness::reaches(L, wm, L.exit_tx, L.exit_ty));
  CHECK(!harness::reaches(L, wm, L.cage_tx, L.cage_ty));
}
