#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon1_room0.h"
#include "game/levels/dungeon1_room1.h"
#include "game/levels/dungeon1_room2.h"
#include <set>
#include <utility>
#include <cstdio>
using namespace logic;

// Whispering Woods (Dungeon 1) — M12 restructure into 3 rooms (entry -> Guardian boss arena ->
// spronk). Standard structural invariants PLUS first-class no-soft-lock invariants that each FAIL
// on a deliberately-broken layout (verified during authoring; see the report). All assert against
// the COMPILED DUNGEON1_ROOM* data.
//
// I4 MIGRATION: retired the private Grid/flood-fill fork onto test/level_harness.h (the shared
// model). D1's kit is bolt + double-jump (no Fire/Ice/Light/Grapple/Stone) and the rooms have no
// gates/hidden platforms, so every query uses a default harness::WorldModel{} (CLIMB_RELIABLE=5).
// Solid-border / room-door-target-resolution / entrance-settle are now covered generically by
// test_all_dungeons.cpp; d1_rooms_solid_border was dropped as a pure duplicate.

static const LevelData* const D1_ROOMS[] = {
    &DUNGEON1_ROOM0_DATA, &DUNGEON1_ROOM1_DATA, &DUNGEON1_ROOM2_DATA };
static constexpr int D1_N = 3;

using RSet = std::set<std::pair<int,int>>;

static bool stands_at(const LevelData&, const RSet& R, int tx, int ty){
    for(int dy = 0; dy <= 1; ++dy)
        for(int lx = tx-harness::PW+1; lx <= tx; ++lx)
            if(R.count({lx, ty+dy})) return true;
    return false;
}
static bool reaches_forward_exit(const LevelData& L, const RSet& R){
    for(int i = 0; i < L.room_door_count; ++i)
        if(stands_at(L, R, L.room_doors[i].tx, L.room_doors[i].ty)) return true;
    if(L.has_cage && stands_at(L, R, L.cage_tx, L.cage_ty)) return true;
    if(L.has_exit && stands_at(L, R, L.exit_tx, L.exit_ty)) return true;
    return false;
}

static int d1_tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }

// ===========================================================================
// Structural invariants
// ===========================================================================
TEST(d1_dungeon_table){
    CHECK_EQ(DUNGEON1_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON1_DUNGEON.start_room, 0);
    CHECK(DUNGEON1_DUNGEON.rooms[0] == &DUNGEON1_ROOM0_DATA);
    CHECK(DUNGEON1_DUNGEON.rooms[1] == &DUNGEON1_ROOM1_DATA);
    CHECK(DUNGEON1_DUNGEON.rooms[2] == &DUNGEON1_ROOM2_DATA);
}

// Room 1 is the boss arena: ≥30 wide, has a boss, and has NO cage / NO exit (the fight ends in code,
// victory opens the onward door). Room 0 is the entry with the Featherleap shrine and NO cage.
// Room 2 is the spronk chamber: has the cage + the exit. No other room has a boss.
TEST(d1_room1_is_boss_arena){
    const LevelData& L = DUNGEON1_ROOM1_DATA;
    REQUIRE(L.boss != nullptr);
    CHECK(L.boss == &D1_DEF);              // the ONE canonical boss symbol
    CHECK_EQ((int)L.boss->phase_count, 2);
    CHECK(!L.has_cage);
    CHECK(!L.has_exit);
    CHECK(L.w >= 30);
}
TEST(d1_only_room1_has_boss){
    CHECK(DUNGEON1_ROOM0_DATA.boss == nullptr);
    CHECK(DUNGEON1_ROOM2_DATA.boss == nullptr);
}
TEST(d1_room0_entry_no_cage_has_featherleap){
    const LevelData& L = DUNGEON1_ROOM0_DATA;
    CHECK(!L.has_cage);
    CHECK_EQ(L.pickup_count, 1);
    CHECK(L.pickups[0].ability == Ability::Featherleap);
}
TEST(d1_room2_has_cage_and_exit){
    const LevelData& L = DUNGEON1_ROOM2_DATA;
    CHECK(L.has_cage);
    CHECK(L.has_exit);
    // Grounded: a solid tile directly below the cage + exit sprite tile (they don't float).
    CHECK(d1_tile(L, L.cage_tx, L.cage_ty+1) == 1);
    CHECK(d1_tile(L, L.exit_tx, L.exit_ty+1) == 1);
}

// Room-doors resolve to a real target room + a matching entrance id; each room has exactly the one
// hub-exit 'Q' (room 0 only). Pins the 0<->1<->2 graph (+ the hub return in room 0). Target-
// resolution is also covered generically by test_all_dungeons.cpp; this test additionally pins the
// D1-specific graph edges (which the registry does not check).
TEST(d1_room_doors_resolve){
    for(int r = 0; r < D1_N; ++r){
        const LevelData& L = *D1_ROOMS[r];
        for(int i = 0; i < L.room_door_count; ++i){
            const RoomDoorSpawn& d = L.room_doors[i];
            if(d.target_room < 0) continue;   // hub-exit 'Q'
            CHECK(d.target_room >= 0 && d.target_room < D1_N);
            const LevelData& T = *D1_ROOMS[d.target_room];
            bool found=false; for(int e=0;e<T.entrance_count;++e) if(T.entrances[e].id==d.target_entrance) found=true;
            CHECK(found);
        }
    }
    auto has_door=[&](const LevelData& L,int tr,int te){
        for(int i=0;i<L.room_door_count;++i)
            if(L.room_doors[i].target_room==tr && L.room_doors[i].target_entrance==te) return true;
        return false;
    };
    CHECK(has_door(DUNGEON1_ROOM0_DATA,1,0));   // entry -> boss arena
    CHECK(has_door(DUNGEON1_ROOM1_DATA,2,0));   // boss arena -> spronk
    CHECK(has_door(DUNGEON1_ROOM1_DATA,0,1));   // boss arena -> back to entry
    CHECK(has_door(DUNGEON1_ROOM2_DATA,1,1));   // spronk -> back to boss arena
    // exactly one hub-exit door, in room 0
    int hub=0; for(int r=0;r<D1_N;++r){ const LevelData& L=*D1_ROOMS[r];
        for(int i=0;i<L.room_door_count;++i) if(L.room_doors[i].target_room==-1) ++hub; }
    CHECK_EQ(hub, 1);
}

// ===========================================================================
// NO-SOFT-LOCK INVARIANTS (first-class). Each verified to FAIL on a broken layout.
// ===========================================================================

// 1. Room 0: from the dungeon-entry spawn, BOTH the Featherleap shrine AND the onward door to room 1
//    are reachable (bolt+double-jump). Earn the ability, then move on. Break test: float the shrine /
//    wall off the door -> unreachable -> RED.
TEST(d1_room0_shrine_and_door_reachable){
    const LevelData& L = DUNGEON1_ROOM0_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable_from(L, wm, L.spawn_tx, L.spawn_ty);
    bool shrine = stands_at(L, R, L.pickups[0].tx, L.pickups[0].ty);
    CHECK(shrine);
    // the onward door (target_room == 1)
    bool door = false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==1 && stands_at(L, R, L.room_doors[i].tx, L.room_doors[i].ty)) door=true;
    CHECK(door);
    std::printf("  [room0] shrine=%s onward-door=%s\n", shrine?"reach":"NO", door?"reach":"NO");
}

// 2. Room 1 (boss arena): from the room-1 entrance the arena floor is traversable and the onward door
//    to room 2 is reachable (the boss fight is in code, not geometry; the arena itself must let the
//    player walk to the exit once the boss is down). Break test: wall off the onward door -> RED.
TEST(d1_room1_arena_traversable_onward_reachable){
    const LevelData& L = DUNGEON1_ROOM1_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable(L, wm);
    bool onward = false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==2 && stands_at(L, R, L.room_doors[i].tx, L.room_doors[i].ty)) onward=true;
    CHECK(onward);
    std::printf("  [room1] onward-door(to room2) reachable=%s\n", onward?"yes":"NO");
}

// 3. Room 2 (spronk): from the room-2 entrance BOTH the caged spronk AND the exit are reachable with
//    ONLY bolt + double-jump (no Fire/Ice/etc.). Break test: float/wall the cage or exit -> RED.
TEST(d1_room2_spronk_and_exit_reachable){
    const LevelData& L = DUNGEON1_ROOM2_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable(L, wm);
    bool c = stands_at(L, R, L.cage_tx, L.cage_ty);
    bool e = stands_at(L, R, L.exit_tx, L.exit_ty);
    CHECK(c); CHECK(e);
    std::printf("  [room2] cage=%s exit=%s\n", c?"reach":"NO", e?"reach":"NO");
}

// 4. No pit / one-way traps: from each room's entrance the player can re-reach a forward exit
//    (a door/cage/exit). With only bolt+double-jump, every room must be solvable AND escapable.
//    Break test: drop the floor out under a room so the entrance can't reach any exit -> RED.
TEST(d1_no_one_way_traps){
    for(int r = 0; r < D1_N; ++r){
        const LevelData& L = *D1_ROOMS[r];
        harness::WorldModel wm{};
        RSet R = (r==0) ? harness::reachable_from(L, wm, L.spawn_tx, L.spawn_ty) : harness::reachable(L, wm);
        bool fwd = reaches_forward_exit(L, R);
        CHECK(fwd);
        std::printf("  [oneway] room %d -> forward exit reachable = %s\n", r, fwd?"yes":"NO");
    }
}
