#include "test_framework.h"
#include "level_harness.h"
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

// M8 retrofit: hub-return door (Q) near the spawn — exactly one, grounded on the main bottom floor.
static int d5_tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(d5_has_hub_return_door){
  const LevelData& L = DUNGEON5_DATA;
  int hub_doors = 0;
  for(int i = 0; i < L.room_door_count; ++i)
    if(L.room_doors[i].target_room == -1) ++hub_doors;
  CHECK_EQ(hub_doors, 1);
}
TEST(d5_hub_door_grounds_on_main_floor){
  // The Q door's 2-wide archway (cols tx, tx+1) must ground on the main bottom floor (row h-2).
  const LevelData& L = DUNGEON5_DATA;
  const int floor_row = L.h - 2;   // row 22 for h=24
  for(int i = 0; i < L.room_door_count; ++i){
    if(L.room_doors[i].target_room != -1) continue;
    for(int dx = 0; dx < 2; ++dx){
      int col = L.room_doors[i].tx + dx;
      int fr = -1;
      for(int y = L.room_doors[i].ty + 1; y < L.h; ++y)
        if(d5_tile(L, col, y) == 1){ fr = y; break; }
      CHECK_EQ(fr, floor_row);
    }
  }
}

// ----------------------------------------------------------------------------
// Task 2.6 — real reachability via the shared harness (was existence-only coverage). Full kit: every
// gate open (Vine at x9, FireWall at x27, CrackedWall at x50) + glide. D5 has NO Water(4) tiles
// anywhere in its data, so water_frozen is a documented no-op here (nothing to freeze) -- included for
// the same reason every other dungeon's full-kit WorldModel sets it, not because this room needs it.
//
// glide=true is NOT a throwaway default here: two solid pillar walls (cols 19-20 and cols 35-36, rows
// 13-21, floor-to-near-ceiling) split the room into three chambers, with an updraft shaft (cols 31-32)
// threading the middle one. The first wall is crossable by an ordinary diagonal jump onto its 2-wide
// top edge (approach from the row-17 ledge, land straddling the corner, walk across, drop the far
// side -- all within CLIMB_RELIABLE, no glide needed). The SECOND wall is not: reaching its top from
// the inner chamber's floor is a 9-tile gain with no intermediate ledge, and reachable::glide-off
// confirms the shrine (past both walls) is reachable ONLY once glide is granted. This lines up exactly
// with the existing d5_combo_carried_power_obstacle test's own comment ("a Glide updraft beat") --
// this room already documented that it expects the player to carry Glide in from D4; Task 2.6 just
// makes that expectation load-bearing instead of asserted-by-comment.
// ----------------------------------------------------------------------------
static harness::WorldModel d5_full_kit_wm(){
  harness::WorldModel wm{};
  for(int i = 0; i < DUNGEON5_DATA.gate_count; ++i) wm.open_gates.insert(i);
  wm.water_frozen = true;   // no-op: D5 has no Water tiles (see comment above)
  wm.glide = true;          // carried from D4 -- required to clear the second pillar wall (see above)
  return wm;
}

TEST(d5_shrine_requires_carried_glide){
  // The Dash shrine sits past BOTH pillar walls. Without glide, reachability stops dead at the second
  // wall (documented above); with the carried Glide kit, it's reachable.
  const LevelData& L = DUNGEON5_DATA;
  REQUIRE(L.pickup_count == 1);
  harness::WorldModel no_glide = d5_full_kit_wm(); no_glide.glide = false;
  CHECK(!harness::reaches(L, no_glide, L.pickups[0].tx, L.pickups[0].ty));
  harness::WorldModel wm = d5_full_kit_wm();
  CHECK(harness::reaches(L, wm, L.pickups[0].tx, L.pickups[0].ty));
}

// NEEDS_CONTEXT (discovered, not guessed): past the CrackedWall gate (x50), a 6-tile-wide Lava run
// spans row 22 cols 54-59 with no stepping stones -- the harness's horizontal double-jump clears a gap
// of at most CLIMB_RELIABLE-1 = 4 tiles reliably (CLIMB_MAX=7 clears exactly 6, but MAX models only the
// edge-apex "must-NOT-bypass" threshold, never a must-reach guarantee). Empirically: the frontier
// reaches the lava's LEFT edge (col 53) at CLIMB_RELIABLE and no further; the far bank only becomes
// reachable at climb_max. This lines up exactly with the shrine granting Dash right before this run
// (src/logic/player.cpp: dash is a ~5-tile gravity-overriding horizontal blink) -- Dash is almost
// certainly the intended crossing, but "dash distances" are explicitly out of scope for this harness
// (Task 2.6 brief). So exit/cage reachability past the lava is NOT asserted here: proving it would
// require inventing a dash-distance movement rule, which risks papering over an actual gap instead of
// reporting it. The two checks below instead pin the exact, honest frontier (reachable up to the near
// bank at RELIABLE; the far bank needs the edge-apex MAX jump, i.e. genuinely NOT reliable without an
// extra ability) so a future dash-distance rule has a concrete regression to extend.
TEST(d5_lava_run_blocks_at_reliable_reach){
  const LevelData& L = DUNGEON5_DATA;
  harness::WorldModel wm = d5_full_kit_wm();
  CHECK(harness::reaches(L, wm, 53, 20));           // near (left) bank: reliably reachable
  CHECK(!harness::reaches(L, wm, L.exit_tx, L.exit_ty));  // far bank: NOT reliably reachable
  CHECK(!harness::reaches(L, wm, L.cage_tx, L.cage_ty));
}
TEST(d5_exit_and_cage_need_more_than_the_edge_apex_jump_alone){
  // Documents that even CLIMB_MAX (the unreliable edge-apex jump) is exactly at the limit here (a
  // 6-tile gap needs a 7-tile leap, k<=CLIMB_MAX) -- i.e. this is NOT comfortably within reach even for
  // a frame-perfect jump, reinforcing that Dash (not a risky jump) is the intended crossing.
  const LevelData& L = DUNGEON5_DATA;
  harness::WorldModel wm = d5_full_kit_wm(); wm.climb_max = true;
  CHECK(harness::reaches(L, wm, L.exit_tx, L.exit_ty));   // only the edge-apex MAX jump gets there
  CHECK(harness::reaches(L, wm, L.cage_tx, L.cage_ty));
}
