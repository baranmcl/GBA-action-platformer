#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon2_room0.h"
#include "game/levels/dungeon2_room1.h"
#include "game/levels/dungeon2_room2.h"
#include <vector>
using namespace logic;

// =============================================================================
// test_level_harness.cpp — self-tests for test/level_harness.h (the shared model).
// The harness is load-bearing (six per-dungeon test forks will migrate onto it), so it
// gets its own tests, including a physics canary that pins CLIMB_* to the real player.
//
// Fixtures are hand-built LevelData: a std::vector<uint8_t> tile buffer + a default-
// aggregate-initialised LevelData whose members we set directly (avoids the ~40-field
// positional init and keeps the maps tiny).
// =============================================================================

namespace {

// A tiny bordered room builder. Owns the tile buffer + optional spawn arrays so the
// LevelData it hands out stays valid for the lifetime of the Fixture.
struct Fixture {
    int w, h;
    std::vector<uint8_t> t;
    std::vector<GateSpawn> gates;
    std::vector<HiddenPlatformSpawn> hidden;
    int spawn_tx = 2, spawn_ty = 1;

    Fixture(int W, int H) : w(W), h(H), t(W*H, (uint8_t)TileKind::Empty) {
        // solid border on all four edges
        for(int x=0; x<w; ++x){ set(x,0,TileKind::Solid); set(x,h-1,TileKind::Solid); }
        for(int y=0; y<h; ++y){ set(0,y,TileKind::Solid); set(w-1,y,TileKind::Solid); }
    }
    void set(int x, int y, TileKind k){ t[y*w+x] = (uint8_t)k; }
    void fill_row(int y, int x0, int x1, TileKind k){ for(int x=x0; x<=x1; ++x) set(x,y,k); }

    LevelData level() const {
        LevelData L{};
        L.tiles = t.data(); L.w = w; L.h = h;
        L.spawn_tx = spawn_tx; L.spawn_ty = spawn_ty;
        if(!gates.empty()){ L.gates = gates.data(); L.gate_count = (int)gates.size(); }
        if(!hidden.empty()){ L.hidden_platforms = hidden.data(); L.hidden_platform_count = (int)hidden.size(); }
        return L;
    }
};

// A room with a solid interior floor at row (h-2) and the standard border below it (h-1).
// The player settles onto the floor with feet at row (h-2)... actually feet rest ON the floor,
// so feet-row F = h-3 when the floor tile is at h-2. We keep it simple: no interior floor row,
// the player rests on the bottom border (row h-1) -> feet row F = h-2.
} // namespace

// ----------------------------------------------------------------------------
// 1. CLIMB height: a 5-up ledge is reachable at RELIABLE; 6-up is not (but IS at MAX);
//    8-up is not reachable even at MAX.
// ----------------------------------------------------------------------------
// A tall bordered room with a single 3-wide ledge offset from the spawn column so the
// player must DIAGONAL-climb onto it. feet-on-floor F = h-2 = 18 (h=20). A ledge whose top
// solid row is L gives a standing feet-row of L-1, i.e. a height gain of F-(L-1).
static Fixture climb_fixture(int gain){
    const int W = 20, H = 20;
    Fixture f(W, H);
    int F = H - 2;                 // 18 — feet row on the bottom-border floor
    int feet = F - gain;           // target standing feet-row on the ledge
    int L = feet + 1;              // ledge solid row (surface directly under the feet)
    // 3-wide ledge at cols 9,10,11 so a body approaching at col 7 (cols 7,8 clear) can
    // diagonal-jump to col 8 and rest supported by the ledge's col-9 edge.
    f.fill_row(L, 9, 11, TileKind::Solid);
    return f;
}

TEST(harness_climb_5up_reachable_reliable){
    Fixture f = climb_fixture(5);
    LevelData L = f.level();
    harness::WorldModel wm{};                    // RELIABLE (climb 5)
    int feet = (L.h-2) - 5;
    CHECK(harness::reaches(L, wm, 10, feet));     // stand on the ledge
}
TEST(harness_climb_6up_unreachable_reliable_reachable_at_max){
    Fixture f = climb_fixture(6);
    LevelData L = f.level();
    int feet = (L.h-2) - 6;
    harness::WorldModel reliable{};
    CHECK(!harness::reaches(L, reliable, 10, feet));   // 6 > CLIMB_RELIABLE(5): NOT reachable
    harness::WorldModel max{}; max.climb_max = true;
    CHECK(harness::reaches(L, max, 10, feet));          // 6 <= CLIMB_MAX(7): reachable at the edge
}
TEST(harness_climb_8up_unreachable_even_at_max){
    Fixture f = climb_fixture(8);
    LevelData L = f.level();
    int feet = (L.h-2) - 8;
    harness::WorldModel max{}; max.climb_max = true;
    CHECK(!harness::reaches(L, max, 10, feet));          // 8 > CLIMB_MAX(7): never reachable
}

// ----------------------------------------------------------------------------
// 2. Horizontal double-jump: a 4-tile gap crosses; a 5-tile gap does not (the D2 move).
// ----------------------------------------------------------------------------
// A run of Water on an interior floor row is a floor-level hazard gap (unstandable, and the
// solid border below leaves no pit to drop into). The 2-wide body hang-lands on the far gap
// column (resting on the first right-floor tile), so a k-tile leap clears (k-1) fully-open
// tiles: `water` water columns need a k=`water` leap and fly OVER (water-1) tiles. With
// CLIMB_RELIABLE=5 the max leap is k=5 -> clears a 4-tile gap; a 5-tile gap needs k=6 -> fails.
static Fixture gap_fixture(int water){
    const int W = 24, H = 12;
    Fixture f(W, H);
    int floor = H - 2;                       // interior floor row (10); border row 11 below
    f.fill_row(floor, 1, W-2, TileKind::Solid);
    int g0 = 8;
    f.fill_row(floor, g0, g0+water-1, TileKind::Water);  // floor-level hazard gap
    return f;
}

TEST(harness_gap_4tile_crosses){
    Fixture f = gap_fixture(5);                 // 5 water cols, k=5 leap -> flies over a 4-tile gap
    LevelData L = f.level();
    harness::WorldModel wm{};
    int feet = (L.h-2) - 1;                     // 9 — feet on the interior floor (row 10)
    CHECK(harness::reaches(L, wm, 16, feet));   // right side reachable over a 4-tile gap
}
TEST(harness_gap_5tile_blocks){
    Fixture f = gap_fixture(6);                 // 6 water cols needs a k=6 leap > CLIMB_RELIABLE
    LevelData L = f.level();
    harness::WorldModel wm{};
    int feet = (L.h-2) - 1;
    CHECK(!harness::reaches(L, wm, 16, feet));  // 5-tile gap exceeds the horizontal reach
}

// ----------------------------------------------------------------------------
// 3. A closed Vine gate blocks; open_gates={0} unblocks.
// ----------------------------------------------------------------------------
static Fixture gate_fixture(){
    const int W = 20, H = 12;
    Fixture f(W, H);
    // Floor = bottom border (feet row 10). A single Vine gate at col 9 divides the room.
    f.gates.push_back(GateSpawn{ 9, 0, GateType::Vine, -1 });
    f.spawn_tx = 2; f.spawn_ty = 1;
    return f;
}

TEST(harness_closed_vine_gate_blocks){
    Fixture f = gate_fixture();
    LevelData L = f.level();
    harness::WorldModel closed{};                 // gate 0 not in open_gates -> filled wall
    int feet = L.h - 2;                           // 10
    CHECK(!harness::reaches(L, closed, 15, feet)); // right side sealed off
}
TEST(harness_open_vine_gate_unblocks){
    Fixture f = gate_fixture();
    LevelData L = f.level();
    harness::WorldModel open{}; open.open_gates.insert(0);
    int feet = L.h - 2;
    CHECK(harness::reaches(L, open, 15, feet));    // gate cleared -> right side reachable
    // reachable_from also floods across the cleared gate from the far side.
    auto R = harness::reachable_from(L, open, 15, feet);
    CHECK(R.count({ 3, feet }) == 1);              // left side reachable from a right-side seed
}

// ----------------------------------------------------------------------------
// 4. Water blocks as a hazard; water_frozen makes the run standable + non-hazard.
// ----------------------------------------------------------------------------
// A 6-tile water run (wider than the 4-tile horizontal reach) in an interior floor.
static Fixture water_fixture(){
    const int W = 24, H = 12;
    Fixture f(W, H);
    int floor = H - 2;                       // interior floor row 10
    f.fill_row(floor, 1, W-2, TileKind::Solid);
    f.fill_row(floor, 8, 13, TileKind::Water);   // 6-tile water run
    return f;
}

TEST(harness_water_blocks_when_unfrozen){
    Fixture f = water_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{};                 // water_frozen = false
    int feet = (L.h-2) - 1;                   // 9
    CHECK(!harness::reaches(L, wm, 16, feet)); // 6-tile hazard gap uncrossable
}
TEST(harness_frozen_water_is_standable){
    Fixture f = water_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{}; wm.water_frozen = true;
    int feet = (L.h-2) - 1;
    CHECK(harness::reaches(L, wm, 16, feet));  // frozen run becomes a standable bridge
    // and the frozen run itself is now standable (feet ON the frozen tiles, non-hazard)
    harness::Grid g = harness::build_grid(L, wm);
    CHECK(g.hazard[(L.h-2)*L.w + 10] == 0);    // no longer a hazard
    CHECK(g.standable[(L.h-2)*L.w + 10] == 1); // standable
}

// ----------------------------------------------------------------------------
// 5. Hidden platform run standable only when hidden_platforms_shown.
// ----------------------------------------------------------------------------
static Fixture hidden_fixture(){
    const int W = 20, H = 12;
    Fixture f(W, H);
    // A 3-wide hidden platform at row 7 (cols 9,10,11); floor = bottom border (feet 10).
    f.hidden.push_back(HiddenPlatformSpawn{ 9, 7, 3 });
    return f;
}

TEST(harness_hidden_platform_unreachable_when_dark){
    Fixture f = hidden_fixture();
    LevelData L = f.level();
    harness::WorldModel dark{};                    // hidden_platforms_shown = false
    CHECK(!harness::reaches(L, dark, 10, 6));       // platform not solid -> cannot stand on it
}
TEST(harness_hidden_platform_reachable_when_revealed){
    Fixture f = hidden_fixture();
    LevelData L = f.level();
    harness::WorldModel lit{}; lit.hidden_platforms_shown = true;
    CHECK(harness::reaches(L, lit, 10, 6));         // revealed -> solid -> reliably reachable
}

// ----------------------------------------------------------------------------
// 6. PHYSICS CANARY — pins CLIMB_* to the REAL logic::Player double-jump.
//    Deterministic fixed-tick schedule; integer asserts. Its FAILURE means the jump
//    physics were retuned and the harness constants must follow.
// ----------------------------------------------------------------------------
TEST(harness_physics_canary_double_jump_apex){
    // Tall bordered shaft so the jump never hits a ceiling. Floor = bottom border.
    const int W = 4, H = 40;
    std::vector<uint8_t> cells(W*H, (uint8_t)TileKind::Empty);
    for(int x=0; x<W; ++x){ cells[x] = (uint8_t)TileKind::Solid; cells[(H-1)*W+x] = (uint8_t)TileKind::Solid; }
    for(int y=0; y<H; ++y){ cells[y*W] = (uint8_t)TileKind::Solid; cells[y*W+(W-1)] = (uint8_t)TileKind::Solid; }
    Tilemap map{ W, H, cells.data() };

    Player p;
    p.body.half_w = Fixed::from_int(8);
    p.body.half_h = Fixed::from_int(16);           // the real 2x4-tile body
    // Start just above the floor (resting pos.y = 312-32 = 280) so the body GROUNDS within a
    // few ticks and refreshes its air-jump BEFORE the scheduled ground jump. (Starting high up
    // meant it was still falling at JUMP1, so on_ground was false and the jump was a no-op.)
    p.body.pos = { Fixed::from_int(8), Fixed::from_int(270) };
    p.body.on_ground = false;
    p.abilities.featherleap = true;                // double jump available

    auto feet_row = [&](){
        return Tilemap::px_to_tile(p.body.pos.y + p.body.half_h + p.body.half_h - Fixed::from_int(1));
    };

    InputFrame none{};
    InputFrame jump{}; jump.jump_pressed = true;

    // Fixed schedule (no wall-clock / no "until apex" loop): settle to the floor (grounded by
    // ~tick 10), ground-jump at tick 12 (on_ground true at the start of that update), then
    // double-jump 17 ticks later at the first-jump apex, tracking the highest feet row.
    const int SETTLE = 12, JUMP1 = 12, JUMP2 = 29, TOTAL = 120;
    int start_feet = 0, apex_feet = 1 << 30;
    for(int f=0; f<TOTAL; ++f){
        const InputFrame& in = (f==JUMP1 || f==JUMP2) ? jump : none;
        p.update(in, map);
        if(f == SETTLE-1) start_feet = feet_row();          // grounded reference
        if(f >= JUMP1) apex_feet = apex_feet < feet_row() ? apex_feet : feet_row();
    }
    int gain = start_feet - apex_feet;                       // tiles gained by the double jump

    std::printf("  [canary] start_feet=%d apex_feet=%d double-jump gain=%d tiles (RELIABLE=%d MAX=%d)\n",
                start_feet, apex_feet, gain, harness::CLIMB_RELIABLE, harness::CLIMB_MAX);

    bool ok = (gain >= harness::CLIMB_RELIABLE && gain <= harness::CLIMB_MAX);
    if(!ok)
        std::printf("  FAIL canary: double-jump apex gain=%d NOT in [%d,%d] -- "
                    "retune harness::CLIMB_* to match player physics\n",
                    gain, harness::CLIMB_RELIABLE, harness::CLIMB_MAX);
    CHECK(ok);
    CHECK(p.body.on_ground);   // came back down and settled (schedule long enough)
}

// ----------------------------------------------------------------------------
// 7. Universal invariants smoke-test against real compiled dungeon data (D2).
//    Guards the void CHECK-based helpers the future test_all_dungeons.cpp will use.
// ----------------------------------------------------------------------------
TEST(harness_universal_invariants_on_d2){
    harness::check_solid_border(DUNGEON2_ROOM0_DATA, "d2-room0");
    harness::check_solid_border(DUNGEON2_ROOM1_DATA, "d2-room1");
    harness::check_solid_border(DUNGEON2_ROOM2_DATA, "d2-room2");
    harness::check_room_doors_resolve(DUNGEON2_DUNGEON, "d2");
    harness::check_entrances_settle_safely(DUNGEON2_ROOM0_DATA, "d2-room0");
    harness::check_entrances_settle_safely(DUNGEON2_ROOM1_DATA, "d2-room1");
    harness::check_entrances_settle_safely(DUNGEON2_ROOM2_DATA, "d2-room2");
}
