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
    std::vector<BoulderSpawn> boulders;
    std::vector<LoosePlatformSpawn> loose;
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
        if(!boulders.empty()){ L.boulders = boulders.data(); L.boulder_count = (int)boulders.size(); }
        if(!loose.empty()){ L.loose_platforms = loose.data(); L.loose_platform_count = (int)loose.size(); }
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
// 5b. Object-state toggles (Task 2.4 review fix): boulders_broken/broken_boulder_idx,
//     cracked_floors_broken, and loose_dropped/dropped_loose_idx each have their own fixture-based
//     self-test, mirroring the water_fixture/gate_fixture style above. No harness self-test existed
//     for any of these before this fix, even though loose_dropped (2.4) and the pre-existing
//     boulders_broken/cracked_floors_broken are load-bearing for every D7/D8/D9 no-strand invariant.
// ----------------------------------------------------------------------------

// A full-height 2-wide boulder wall at cols 9-10 (rows 1..h-3, mirroring exactly how a closed Vine/
// DarkVeil gate fills a corridor in build_grid -- see gate_fixture above). A single floor-level boulder
// tile is NOT enough: the harness's horizontal double-jump rule only inspects rows ABOVE the floor when
// checking a leap's headroom, and standing on TOP of a short boulder pile is itself a valid resting
// surface (surf() doesn't care what made the tile solid) — either lets the player bypass a short pile.
// Filling every row in the corridor's clearance leaves no row left to stand on or climb through.
static Fixture boulder_fixture(){
    const int W = 20, H = 12;
    Fixture f(W, H);
    for(int ty = 1; ty < H-2; ++ty){
        f.boulders.push_back(BoulderSpawn{ 9,  ty });
        f.boulders.push_back(BoulderSpawn{ 10, ty });
    }
    return f;
}
// Boulder index layout from boulder_fixture(): pairs (9,ty),(10,ty) for ty=1..h-3, in row order — EVEN
// indices are column 9, ODD indices are column 10.

TEST(harness_boulder_intact_blocks_corridor){
    Fixture f = boulder_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{};                              // boulders_broken=false, idx set empty
    CHECK(!harness::reaches(L, wm, 15, L.h-2));             // far side sealed
}
TEST(harness_boulder_broken_blanket_unblocks_corridor){
    Fixture f = boulder_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{}; wm.boulders_broken = true;
    CHECK(harness::reaches(L, wm, 15, L.h-2));              // fully cleared -> far side reachable
}
TEST(harness_boulder_broken_per_index_unblocks_corridor){
    Fixture f = boulder_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{};
    for(int i=0; i<(int)f.boulders.size(); ++i) wm.broken_boulder_idx.insert(i);   // every tile, by index
    CHECK(harness::reaches(L, wm, 15, L.h-2));
}
TEST(harness_boulder_partial_index_clear_still_blocks){
    Fixture f = boulder_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{};
    // Clear only column 9's tiles (every even index); column 10 stays a full, unbroken wall. fits()
    // needs BOTH columns clear, so the corridor stays fully sealed even though half its tiles are gone
    // -- this is the case the retired-fork-shaped per-object test in test_dungeon7_level.cpp guards:
    // clearing one object must not be conflated with clearing all of them.
    for(int i=0; i<(int)f.boulders.size(); ++i) if(i % 2 == 0) wm.broken_boulder_idx.insert(i);
    CHECK(!harness::reaches(L, wm, 15, L.h-2));             // column 10 still solid -> still sealed
}

// A single interior floor row with a 2-wide gap; a cracked floor gate fills the gap (solid) until Stone
// smashes it, sealing a shaft down to an otherwise fully-enclosed lower chamber.
static constexpr int CRACKED_FLOOR_ROW = 8;
static Fixture cracked_fixture(){
    const int W = 20, H = 16;
    Fixture f(W, H);
    f.fill_row(CRACKED_FLOOR_ROW, 1, W-2, TileKind::Solid);
    // The crack tiles are Empty in tiles[] (content symbols, not compiled-solid) -- the gate array
    // makes them solid at runtime, mirroring the compiled dungeon format (see build_grid's comment).
    f.set(10, CRACKED_FLOOR_ROW, TileKind::Empty);
    f.set(11, CRACKED_FLOOR_ROW, TileKind::Empty);
    f.gates.push_back(GateSpawn{ 10, CRACKED_FLOOR_ROW, GateType::CrackedFloor, -1 });
    f.gates.push_back(GateSpawn{ 11, CRACKED_FLOOR_ROW, GateType::CrackedFloor, -1 });
    return f;
}

TEST(harness_cracked_floor_intact_seals_shaft){
    Fixture f = cracked_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{};                               // cracked_floors_broken=false
    CHECK(!harness::reaches(L, wm, 5, L.h-2));               // lower chamber unreachable (sealed)
}
TEST(harness_cracked_floor_broken_opens_shaft){
    Fixture f = cracked_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{}; wm.cracked_floors_broken = true;
    CHECK(harness::reaches(L, wm, 5, L.h-2));                // fall-through -> lower chamber reachable
}

// A loose-platform run authored directly over a pit in an interior floor: intact, it fills the pit (the
// player stands on TOP of it); dropped, the pit opens and the platform falls to rest atop the true
// floor below (the border), bridging that lower position and opening the chamber beneath the pit.
static Fixture loose_fixture(){
    const int W = 20, H = 16;
    Fixture f(W, H);
    f.fill_row(CRACKED_FLOOR_ROW, 1, W-2, TileKind::Solid);   // reuse the same floor row as cracked_fixture
    for(int dx=0; dx<4; ++dx) f.set(8+dx, CRACKED_FLOOR_ROW, TileKind::Empty);   // the pit (content symbol)
    f.loose.push_back(LoosePlatformSpawn{ 8, CRACKED_FLOOR_ROW, 4 });
    return f;
}

TEST(harness_loose_platform_intact_solid_on_top){
    Fixture f = loose_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{};                                // loose_dropped=false, idx set empty
    CHECK(harness::reaches(L, wm, 9, CRACKED_FLOOR_ROW-1));  // stands on the intact platform (upper pos)
    CHECK(!harness::reaches(L, wm, 3, L.h-2));                // pit plugged -> lower chamber sealed
}
TEST(harness_loose_platform_dropped_blanket_bridges_and_opens_below){
    Fixture f = loose_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{}; wm.loose_dropped = true;
    CHECK(harness::reaches(L, wm, 3, L.h-2));                 // fall-through -> lower chamber reachable
    harness::Grid g = harness::build_grid(L, wm);
    CHECK(g.solid[(L.h-2)*L.w + 9] == 1);                     // platform rested atop the true floor
}
TEST(harness_loose_platform_dropped_per_index_bridges_and_opens_below){
    Fixture f = loose_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{}; wm.dropped_loose_idx.insert(0);
    CHECK(harness::reaches(L, wm, 3, L.h-2));
    harness::Grid g = harness::build_grid(L, wm);
    CHECK(g.solid[(L.h-2)*L.w + 9] == 1);
}

// ----------------------------------------------------------------------------
// 5c. Task 2.6 — Wind tiles (WindLeft/WindRight) are passable background, exactly like before the
//     explicit build_grid case was added: no push force is modeled (conservative; see the header
//     comment on the WindLeft/WindRight case in build_grid). This pins that adding the explicit
//     no-op case didn't change the model.
// ----------------------------------------------------------------------------
TEST(harness_wind_tiles_are_passable_non_hazard){
    Fixture f(12, 12);
    f.set(5, 6, TileKind::WindLeft);
    f.set(6, 6, TileKind::WindRight);
    LevelData L = f.level();
    harness::Grid g = harness::build_grid(L, harness::WorldModel{});
    for(int x : {5, 6}){
        CHECK(g.solid[6*L.w + x] == 0);
        CHECK(g.hazard[6*L.w + x] == 0);
        CHECK(g.standable[6*L.w + x] == 0);
    }
}

// ----------------------------------------------------------------------------
// 5d. Task 2.6 — Updraft lift (glide). A contiguous vertical Updraft run at cols 9-10, rows 10-27, with
//     a landing ledge at row 9 (feet row 8) directly above the run's top (row 10). The floor-to-ledge
//     gain (28 -> 8 = 20 tiles) is far beyond CLIMB_MAX(7); only glide=true crosses it.
// ----------------------------------------------------------------------------
static Fixture updraft_fixture(){
    const int W = 20, H = 30;
    Fixture f(W, H);
    // OneWay, not Solid: a Solid ledge in the SAME column as the shaft would fail fits() one row
    // below the ledge (the straight-up climb loop walks every intermediate feet-row, including the
    // ledge's own row, and a body can never "fit" feet-at-a-solid-tile) -- exactly like D4's real
    // updraft shaft, whose landing platform is a OneWay tile for this reason (see dungeon4.h row 7).
    f.fill_row(9, 8, 11, TileKind::OneWay);             // landing ledge (feet row 8)
    for(int y = 10; y <= 27; ++y){ f.set(9, y, TileKind::Updraft); f.set(10, y, TileKind::Updraft); }
    return f;
}
TEST(harness_updraft_blocks_without_glide){
    Fixture f = updraft_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{};                        // glide=false
    CHECK(!harness::reaches(L, wm, 10, 8));           // floor-to-ledge gain (20) needs the lift
}
TEST(harness_updraft_lifts_with_glide){
    Fixture f = updraft_fixture();
    LevelData L = f.level();
    harness::WorldModel wm{}; wm.glide = true;
    CHECK(harness::reaches(L, wm, 10, 8));            // glide rides the lift to the ledge
    // The updraft tiles themselves stay non-solid/non-hazard/non-standable background either way.
    harness::Grid g = harness::build_grid(L, wm);
    CHECK(g.solid[15*L.w + 9] == 0);
    CHECK(g.hazard[15*L.w + 9] == 0);
    CHECK(g.standable[15*L.w + 9] == 0);
}

// ----------------------------------------------------------------------------
// 5e. Task 2.6 — Grapple anchors. An anchor's own tile is its landing cell (guaranteed solid support
//     directly below, mirroring GrappleState::snap_to_ledge). GRAPPLE_RANGE(6) is Chebyshev from the
//     player's BODY-CENTRE tile (logic/grapple.h); in this harness's FEET-row terms that is a 7-tile
//     vertical allowance (see the GRAPPLE_RANGE comment in level_harness.h) -- gap=7 is the boundary.
// ----------------------------------------------------------------------------
static Fixture grapple_fixture(int gap){
    const int W = 20, H = 20;
    Fixture f(W, H);
    int feet = H - 2;                        // 18
    int ay = feet - gap;
    f.set(10, ay, TileKind::GrapplePoint);
    f.fill_row(ay+1, 10, 11, TileKind::Solid);   // every anchor has solid directly below it
    return f;
}
TEST(harness_grapple_anchor_in_range_needs_grapple){
    Fixture f = grapple_fixture(7);              // right at the feet-row boundary
    LevelData L = f.level();
    int ay = (L.h-2) - 7;
    harness::WorldModel none{};                   // grapple=false
    CHECK(!harness::reaches(L, none, 10, ay));    // 7 > CLIMB_RELIABLE(5): ordinary climb can't reach it
    harness::WorldModel gr{}; gr.grapple = true;
    CHECK(harness::reaches(L, gr, 10, ay));        // grapple range covers it
}
TEST(harness_grapple_anchor_out_of_range_stays_unreachable){
    Fixture f = grapple_fixture(8);               // one tile beyond grapple range
    LevelData L = f.level();
    int ay = (L.h-2) - 8;
    harness::WorldModel gr{}; gr.grapple = true;
    CHECK(!harness::reaches(L, gr, 10, ay));       // out of range even with grapple
}
TEST(harness_grapple_anchor_within_climb_reachable_without_grapple){
    // OR-clause: an anchor within ordinary CLIMB_RELIABLE reach needs no grapple ability at all --
    // GrapplePoint tiles are plain passable background, so ordinary climb already applies to them.
    Fixture f = grapple_fixture(5);
    LevelData L = f.level();
    int ay = (L.h-2) - 5;
    harness::WorldModel none{};
    CHECK(harness::reaches(L, none, 10, ay));
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
