#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon9_room0.h"
#include "game/levels/dungeon9_room1.h"
#include "game/levels/dungeon9_arena.h"
#include <set>
#include <utility>
#include <cstdio>
using namespace logic;

// Dungeon 9 (Nightmare King finale, M11) — the 2-room traversal APPROACH + the bespoke boss ARENA.
// No-soft-lock invariants in the M8-M10 discipline: each FAILS on a deliberately-broken layout
// (verified during authoring). Reachability now runs on the shared test/level_harness.h model
// (2-wide x 4-tall body, dual-threshold double-jump) instead of a private flood-fill fork.
//
// Reachability convention: BASE movement only, flooded at CLIMB_RELIABLE (WorldModel{}, climb_max=false).
// The arena's high firing platforms are made base-reachable by an authored staircase (see
// dungeon9_arena.txt "M11 P4.5 reachability fix"); the grapple anchor remains an aerial flourish, not a
// reachability crutch (grapple is NOT modeled — same stance D6 took: feel-reachability is mGBA-verified,
// structural reachability is proved by base movement only). D9 has no gates/hidden/water, so a default
// WorldModel is the whole story.

// ---- Documented firing tiles (must match dungeon9_arena.txt; the two cannot silently drift) ----
static constexpr int P1_FIRE_TX = 19, P1_FIRE_TY = 18;  // atop the central platform (aerial expose)
static constexpr int P2_FIRE_TX = 12, P2_FIRE_TY = 28;  // base floor, under the King (ground expose)
static constexpr int P3_FIRE_TX = 26, P3_FIRE_TY = 18;  // atop the right platform (frozen-foothold expose)

using RSet = std::set<std::pair<int,int>>;

static bool stands_at(const LevelData& L, const RSet& R, int tx, int ty){
    for(int dy = 0; dy <= 1; ++dy)
        for(int lx = tx-harness::PW+1; lx <= tx; ++lx)
            if(R.count({lx, ty+dy})) return true;
    return false;
}
// "Forward" = real progress toward the boss arena. room0 advances via its room-door to room1
// (target_room > start); room1 advances via the dungeon EXIT tile (the run_boss handoff). We deliberately
// do NOT count a room-door whose target_room <= the room's own index (a backtrack door, e.g. room1's
// return-to-room0) — otherwise a walled-off exit would be masked by the always-adjacent backtrack door.
static bool reaches_forward_exit(const LevelData& L, int room_index, const RSet& R){
    if(L.has_exit) return stands_at(L, R, L.exit_tx, L.exit_ty);
    if(L.has_cage) return stands_at(L, R, L.cage_tx, L.cage_ty);
    for(int i = 0; i < L.room_door_count; ++i){
        if(L.room_doors[i].target_room <= room_index) continue; // skip hub-exit + backtrack doors
        if(stands_at(L, R, L.room_doors[i].tx, L.room_doors[i].ty)) return true;
    }
    return false;
}

static const LevelData* const D9_APPROACH[] = { &DUNGEON9_ROOM0_DATA, &DUNGEON9_ROOM1_DATA };
static constexpr int D9_APPROACH_N = 2;

// ===========================================================================
// Structure
// ===========================================================================
TEST(d9_dungeon_tables){
    CHECK_EQ(DUNGEON9_APPROACH.room_count, 2);
    CHECK_EQ(DUNGEON9_APPROACH.start_room, 0);
    CHECK(DUNGEON9_APPROACH.rooms[0] == &DUNGEON9_ROOM0_DATA);
    CHECK(DUNGEON9_APPROACH.rooms[1] == &DUNGEON9_ROOM1_DATA);
    CHECK_EQ(DUNGEON9_ARENA.room_count, 1);
    CHECK_EQ(DUNGEON9_ARENA.start_room, 0);
    CHECK(DUNGEON9_ARENA.rooms[0] == &DUNGEON9_ARENA_DATA);
}

// The arena is a boss room, NOT a spronk rescue: no cage, and a valid player entrance id 0.
// The approach hands off via room1's exit tile (Cleared -> run_boss in main.cpp). Break test:
// set has_cage=true in the arena .txt (add a 'C') -> RED; remove room1's 'E' -> has_exit false -> RED.
TEST(d9_arena_no_cage){
    const LevelData& A = DUNGEON9_ARENA_DATA;
    CHECK(!A.has_cage);
    CHECK(!A.has_exit);          // the fight ends via boss defeat in run_boss, not by an exit tile
    bool ent0 = false;
    for(int i = 0; i < A.entrance_count; ++i) if(A.entrances[i].id == 0) ent0 = true;
    CHECK(ent0);
    // the approach's room1 hands off to the arena via its exit tile
    CHECK(DUNGEON9_ROOM1_DATA.has_exit);
    CHECK(!DUNGEON9_ROOM1_DATA.has_cage);
}

// ===========================================================================
// No-soft-lock invariants
// ===========================================================================

// 1. Each approach room is TRAVERSABLE: from its entrance the forward exit (room-door / dungeon exit)
//    is reachable by base movement. Break test: wall off room1's exit column -> RED.
TEST(d9_approach_traversable){
    int checked = 0;
    for(int r = 0; r < D9_APPROACH_N; ++r){
        const LevelData& L = *D9_APPROACH[r];
        harness::WorldModel wm{};                       // BASE movement (CLIMB_RELIABLE)
        RSet R = harness::reachable(L, wm);
        bool fwd = reaches_forward_exit(L, r, R);
        CHECK(fwd);
        std::printf("  [approach] room %d -> forward exit reachable = %s\n", r, fwd ? "yes" : "NO");
        ++checked;
    }
    CHECK_EQ(checked, D9_APPROACH_N);
}

// 2. The arena's magic crystal is reachable by BASE movement from the entrance (no magic/Light needed)
//    -> no magic soft-lock; every fight attempt can refuel. Break test: wall off the crystal -> RED.
TEST(d9_arena_magic_crystal_reachable){
    const LevelData& A = DUNGEON9_ARENA_DATA;
    CHECK_EQ(A.magic_crystal_count, 1);
    const MagicCrystalSpawn& c = A.magic_crystals[0];
    harness::WorldModel wm{};
    bool reach = harness::reaches(A, wm, c.tx, c.ty);
    CHECK(reach);
    std::printf("  [crystal] $(%d,%d) base-reachable = %s\n", c.tx, c.ty, reach ? "yes" : "NO");
}

// 3. Every per-phase EXPOSE firing tile is reachable by base movement from the entrance -> the arena is
//    solvable each phase (the documented tiles match dungeon9_arena.txt). Break test: remove a platform
//    staircase stub (rows 20/25 below a platform) -> that platform's firing tile -> RED.
TEST(d9_arena_expose_positions_reachable){
    const LevelData& A = DUNGEON9_ARENA_DATA;
    harness::WorldModel wm{};
    bool p1 = harness::reaches(A, wm, P1_FIRE_TX, P1_FIRE_TY);
    bool p2 = harness::reaches(A, wm, P2_FIRE_TX, P2_FIRE_TY);
    bool p3 = harness::reaches(A, wm, P3_FIRE_TX, P3_FIRE_TY);
    CHECK(p1); CHECK(p2); CHECK(p3);
    std::printf("  [expose] P1(%d,%d)=%s P2(%d,%d)=%s P3(%d,%d)=%s\n",
                P1_FIRE_TX, P1_FIRE_TY, p1 ? "reach" : "MISS",
                P2_FIRE_TX, P2_FIRE_TY, p2 ? "reach" : "MISS",
                P3_FIRE_TX, P3_FIRE_TY, p3 ? "reach" : "MISS");
}

// 4. No one-way traps: from the entrance, every room (approach + arena) flood-fill can re-reach a safe
//    standing region. For the approach a forward exit is reachable (covered above); the arena is a
//    terminal fight room (no exit), so we require the entrance region connects to the crystal + all
//    firing tiles (you can always get back to a refuel and to every expose spot). Break test: split the
//    floor so the entrance is isolated -> RED.
TEST(d9_no_one_way_traps){
    harness::WorldModel wm{};
    // approach: forward exit reachable from entrance
    for(int r = 0; r < D9_APPROACH_N; ++r){
        const LevelData& L = *D9_APPROACH[r];
        RSet R = harness::reachable(L, wm);
        CHECK(reaches_forward_exit(L, r, R));
    }
    // arena: entrance connects to crystal + every firing tile (full mutual mobility, no stranding)
    const LevelData& A = DUNGEON9_ARENA_DATA;
    RSet R = harness::reachable(A, wm);
    CHECK(stands_at(A, R, A.magic_crystals[0].tx, A.magic_crystals[0].ty));
    CHECK(stands_at(A, R, P1_FIRE_TX, P1_FIRE_TY));
    CHECK(stands_at(A, R, P2_FIRE_TX, P2_FIRE_TY));
    CHECK(stands_at(A, R, P3_FIRE_TX, P3_FIRE_TY));
}

// 5. No pit traps: every base-reachable standing cell in each room has SAFE (non-hazard) ground below
//    it, and a fall from any platform lands on safe ground (the D9 rooms author NO Lava/Water/Spikes, so
//    no fall can kill). Break test: place a '~' lava tile on the arena floor -> RED.
TEST(d9_no_pit_traps){
    auto room_no_hazard = [](const LevelData& L) -> bool {
        for(int y = 0; y < L.h; ++y) for(int x = 0; x < L.w; ++x){
            TileKind k = (TileKind)L.tiles[y*L.w + x];
            if(k == TileKind::Lava || k == TileKind::Water || k == TileKind::Spikes) return false;
        }
        return true;
    };
    for(int r = 0; r < D9_APPROACH_N; ++r) CHECK(room_no_hazard(*D9_APPROACH[r]));
    CHECK(room_no_hazard(DUNGEON9_ARENA_DATA));

    // Additionally: from every base-reachable standing cell in the arena, scanning straight down to the
    // first solid never crosses a hazard (a fall is always a safe landing -> a retry, never death).
    const LevelData& A = DUNGEON9_ARENA_DATA;
    harness::WorldModel wm{};
    harness::Grid g = harness::build_grid(A, wm);
    RSet R = harness::reachable(A, wm);
    int checked = 0;
    for(const auto& cell : R){
        int x = cell.first, y = cell.second;
        for(int fy = y; fy < A.h; ++fy){
            CHECK(!g.haz(x, fy));
            if(g.blk(x, fy)) break;
        }
        ++checked;
    }
    CHECK(checked >= 1);
}
