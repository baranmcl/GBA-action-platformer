#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon3_room0.h"
#include "game/levels/dungeon3_room1.h"
#include "game/levels/dungeon3_room2.h"
#include <set>
#include <utility>
#include <cstdio>
using namespace logic;

// Frost Hollow (Dungeon 3) — M14 restructure into 3 rooms (puzzle -> Coldforge Twins boss arena ->
// spronk). Standard structural invariants PLUS first-class no-soft-lock invariants that each FAIL
// on a deliberately-broken layout (verified during authoring; see the phase report). All assert
// against the COMPILED DUNGEON3_ROOM* data.
//
// I4 MIGRATION: retired the private D3Grid/flood-fill fork onto test/level_harness.h (the shared
// model). D3's kit is bolt + Fire (needed to counter the ice head) entering the room; Ice is earned
// INSIDE room 0. Rooms 1-2 are flat-floor arenas with no gates, so a default harness::WorldModel{}
// suffices there. Solid-border / room-door-target-resolution / entrance-settle are now covered
// generically by test_all_dungeons.cpp.

using RSet = std::set<std::pair<int,int>>;

static bool stands_at(const LevelData&, const RSet& R, int tx, int ty){
    for(int dy = 0; dy <= 1; ++dy)
        for(int lx = tx-harness::PW+1; lx <= tx; ++lx)
            if(R.count({lx, ty+dy})) return true;
    return false;
}

// ===========================================================================
// Structural invariants
// ===========================================================================
TEST(d3_dungeon_table){
    CHECK_EQ(DUNGEON3_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON3_DUNGEON.start_room, 0);
}
TEST(d3_room1_is_boss_arena_no_crystal){
    const LevelData& L = DUNGEON3_ROOM1_DATA;
    CHECK(L.boss == &D3_DEF);
    CHECK_EQ((int)L.boss->phase_count, 2);
    CHECK(!L.has_cage); CHECK(!L.has_exit);
    CHECK_EQ(L.magic_crystal_count, 0);   // magic comes from block-to-charge (Fire+Ice), not a crystal
}
TEST(d3_only_room1_has_boss){
    CHECK(DUNGEON3_ROOM0_DATA.boss == nullptr);
    CHECK(DUNGEON3_ROOM2_DATA.boss == nullptr);
}
TEST(d3_room0_has_ice_shrine_no_cage){
    const LevelData& L = DUNGEON3_ROOM0_DATA;
    CHECK(!L.has_cage);
    bool ice=false; for(int i=0;i<L.pickup_count;++i) if(L.pickups[i].ability==Ability::Ice) ice=true;
    CHECK(ice);   // Ice earned BEFORE the boss (needed to counter the fire head)
}
TEST(d3_room2_has_cage_and_exit){
    const LevelData& L = DUNGEON3_ROOM2_DATA;
    CHECK(L.has_cage); CHECK(L.has_exit);
}

// ===========================================================================
// NO-SOFT-LOCK INVARIANTS (first-class). Each verified to FAIL on a broken layout.
// ===========================================================================

// 1. Room 1 (boss arena): from the room-1 entrance the arena floor is traversable and the onward
//    door to room 2 is reachable. Break test: wall off the onward door -> RED.
TEST(d3_room1_onward_door_reachable){
    const LevelData& L = DUNGEON3_ROOM1_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable(L, wm);
    bool onward=false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==2 && stands_at(L, R, L.room_doors[i].tx, L.room_doors[i].ty)) onward=true;
    CHECK(onward);
    std::printf("  [d3-room1] onward-door(to room2)=%s\n", onward?"reach":"NO");
}

// 2. Room 2 (spronk): from the room-2 entrance BOTH the caged spronk AND the exit are reachable.
//    Break test: float/wall the cage or exit -> RED.
TEST(d3_room2_spronk_and_exit_reachable){
    const LevelData& L = DUNGEON3_ROOM2_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable(L, wm);
    bool c = stands_at(L, R, L.cage_tx, L.cage_ty);
    bool e = stands_at(L, R, L.exit_tx, L.exit_ty);
    CHECK(c); CHECK(e);
    std::printf("  [d3-room2] cage=%s exit=%s\n", c?"reach":"NO", e?"reach":"NO");
}

// 3. Room 0's hub-return 'Q' is reachable from spawn (unlike D2's, D3's spawn/door content row sits
//    close enough to the true floor that no ground_below snap is needed — confirmed empirically
//    against the compiled data). The onward door to room 1 exists structurally (reachability is now
//    PROVEN, not exempted, by the staged puzzle-solvability chain below).
TEST(d3_room0_hub_return_reachable_and_onward_exists){
    const LevelData& L = DUNGEON3_ROOM0_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable_from(L, wm, L.spawn_tx, L.spawn_ty);
    bool hub=false, onward=false;
    for(int i=0;i<L.room_door_count;++i){
        if(L.room_doors[i].target_room==-1 && stands_at(L, R, L.room_doors[i].tx, L.room_doors[i].ty)) hub=true;
        if(L.room_doors[i].target_room==1) onward=true;
    }
    CHECK(hub); CHECK(onward);
    std::printf("  [d3-room0] hub-return=%s onward-exists=%s\n", hub?"reach":"NO", onward?"yes":"NO");
}

// ===========================================================================
// ROOM 0 — STAGED PUZZLE-SOLVABILITY PROOF (I4, D2 decision). Removes the exemption this test used
// to document (the old comment read: "Room 0 is a spell-gated puzzle (not flood-fill-traversable)
// ... the accepted D2 deviation"). Frost Hollow room 0 has NO plates/buttons/braziers (all counts
// are 0 in the compiled data) — its puzzle is entirely GATE-gated: Vine (Fire) -> Ice shrine -> a
// Water hazard run + FireWall gate (both Ice) -> a second Water hazard run -> an Ice-type gate
// (Fire, already owned) -> the onward door. Gate indices are found by TYPE in the room's COMPILED
// gates[] array (not hardcoded indices), so a re-layout keeps this test honest. D3's kit entering the
// room is bolt+Fire (see file header), so Vine/Ice-type gates are legitimately open()-able the whole
// time; only Ice (earned at the shrine) is a genuine mid-room acquisition, gating the FireWall gate
// AND both raw-tile Water hazard runs (harness::WorldModel::water_frozen).
// ===========================================================================
namespace {
struct D3Puzzle {
    const LevelData& L;
    int vine_idx = -1, firewall_idx = -1, ice_idx = -1;
    int onward_tx = -1, onward_ty = -1;
    D3Puzzle() : L(DUNGEON3_ROOM0_DATA) {
        for(int i=0;i<L.gate_count;++i){
            if(L.gates[i].type==GateType::Vine)     vine_idx=i;
            if(L.gates[i].type==GateType::FireWall) firewall_idx=i;
            if(L.gates[i].type==GateType::Ice)      ice_idx=i;
        }
        for(int i=0;i<L.room_door_count;++i)
            if(L.room_doors[i].target_room==1){ onward_tx=L.room_doors[i].tx; onward_ty=L.room_doors[i].ty; }
    }
};
} // namespace

// Stage 1: the Ice shrine (the room's first trigger/reward) sits directly behind the Vine gate.
// Vine requires Fire, which the player already owns entering D3 (see file header) — so it is
// legitimately open()-able at spawn. CLOSED, the shrine is unreachable even at the edge apex jump
// (CLIMB_MAX); OPENED, it is reliably reachable.
TEST(d3_room0_stage1_ice_shrine_gated_by_vine){
    D3Puzzle P;
    const LevelData& L = P.L;
    REQUIRE(P.vine_idx >= 0);
    REQUIRE(L.pickup_count >= 1);

    harness::WorldModel closed{}; closed.climb_max = true;
    RSet seen_closed = harness::reachable_from(L, closed, L.spawn_tx, L.spawn_ty);
    bool reachable_closed = stands_at(L, seen_closed, L.pickups[0].tx, L.pickups[0].ty);
    CHECK(!reachable_closed);

    harness::WorldModel open{}; open.open_gates.insert(P.vine_idx);   // Fire already owned entering D3
    RSet seen_open = harness::reachable_from(L, open, L.spawn_tx, L.spawn_ty);
    bool reachable_open = stands_at(L, seen_open, L.pickups[0].tx, L.pickups[0].ty);
    CHECK(reachable_open);
    std::printf("  [d3-stage1] Ice shrine(%d,%d) via Vine gate: closed=%s open=%s\n",
                L.pickups[0].tx, L.pickups[0].ty,
                reachable_closed?"REACHABLE(bad)":"gated", reachable_open?"reachable":"STILL-GATED(bad)");
}

// Stage 2 (final): past the shrine sit two raw Water hazard runs (need Ice to freeze — earned at the
// shrine) and a FireWall gate (also cleared by Ice) then an Ice-type gate (cleared by Fire, already
// owned). Each requirement is proven independently necessary: with any ONE of {water_frozen,
// FireWall open, Ice-type-gate open} missing, the onward door stays unreachable even at CLIMB_MAX.
// With all satisfied, it is reliably reachable — the last link from spawn to room 1.
TEST(d3_room0_stage2_onward_door_requires_freeze_and_both_gates){
    D3Puzzle P;
    const LevelData& L = P.L;
    REQUIRE(P.firewall_idx >= 0); REQUIRE(P.ice_idx >= 0);
    REQUIRE(P.onward_tx >= 0);

    // (a) gates open (Fire-only path) but water NOT frozen -> still gated (the raw Water hazard runs
    //     are the real Ice requirement, not just the FireWall gate).
    harness::WorldModel no_freeze{};
    no_freeze.open_gates.insert(P.vine_idx); no_freeze.open_gates.insert(P.firewall_idx); no_freeze.open_gates.insert(P.ice_idx);
    no_freeze.climb_max = true;
    RSet seen_no_freeze = harness::reachable_from(L, no_freeze, L.spawn_tx, L.spawn_ty);
    CHECK(!stands_at(L, seen_no_freeze, P.onward_tx, P.onward_ty));

    // (b) water frozen + Vine open but FireWall CLOSED -> still gated.
    harness::WorldModel no_firewall{};
    no_firewall.open_gates.insert(P.vine_idx); no_firewall.water_frozen = true; no_firewall.climb_max = true;
    RSet seen_no_firewall = harness::reachable_from(L, no_firewall, L.spawn_tx, L.spawn_ty);
    CHECK(!stands_at(L, seen_no_firewall, P.onward_tx, P.onward_ty));

    // (c) water frozen + Vine + FireWall open but the Ice-type gate CLOSED -> still gated.
    harness::WorldModel no_ice_gate{};
    no_ice_gate.open_gates.insert(P.vine_idx); no_ice_gate.open_gates.insert(P.firewall_idx);
    no_ice_gate.water_frozen = true; no_ice_gate.climb_max = true;
    RSet seen_no_ice_gate = harness::reachable_from(L, no_ice_gate, L.spawn_tx, L.spawn_ty);
    CHECK(!stands_at(L, seen_no_ice_gate, P.onward_tx, P.onward_ty));

    // (d) everything satisfied -> reliably reachable.
    harness::WorldModel open{};
    open.open_gates.insert(P.vine_idx); open.open_gates.insert(P.firewall_idx); open.open_gates.insert(P.ice_idx);
    open.water_frozen = true;
    RSet seen_open = harness::reachable_from(L, open, L.spawn_tx, L.spawn_ty);
    bool reachable_open = stands_at(L, seen_open, P.onward_tx, P.onward_ty);
    CHECK(reachable_open);

    std::printf("  [d3-stage2] onward door(%d,%d): no-freeze=gated no-firewall=gated no-ice-gate=gated all-clear=%s\n",
                P.onward_tx, P.onward_ty, reachable_open?"reachable":"STILL-GATED(bad)");
}
