#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon2_room0.h"
#include "game/levels/dungeon2_room1.h"
#include "game/levels/dungeon2_room2.h"
#include <set>
#include <utility>
#include <cstdio>
using namespace logic;

// Ember Caverns (Dungeon 2) — M13 restructure into 3 rooms (entry -> Slagshell boss arena ->
// spronk). Standard structural invariants PLUS first-class no-soft-lock invariants that each FAIL
// on a deliberately-broken layout (verified during authoring; see the phase report). All assert
// against the COMPILED DUNGEON2_ROOM* data.
//
// I4 MIGRATION: retired the private D2Grid/flood-fill fork onto test/level_harness.h (the shared
// model). D2's kit is bolt + double-jump + Fire (the shrine is in room 0). Rooms 1-2 are flat-floor
// arenas with no gates, so a default harness::WorldModel{} suffices there. Solid-border / room-door-
// target-resolution / entrance-settle are now covered generically by test_all_dungeons.cpp;
// d2_rooms_solid_border was dropped as a pure duplicate.

static const LevelData* const D2_ROOMS[] = {
    &DUNGEON2_ROOM0_DATA, &DUNGEON2_ROOM1_DATA, &DUNGEON2_ROOM2_DATA };
static constexpr int D2_N = 3;

using RSet = std::set<std::pair<int,int>>;

static bool stands_at(const LevelData&, const RSet& R, int tx, int ty){
    for(int dy = 0; dy <= 1; ++dy)
        for(int lx = tx-harness::PW+1; lx <= tx; ++lx)
            if(R.count({lx, ty+dy})) return true;
    return false;
}
// Ground-snapped door check (mirrors scene_dungeon.cpp floor_row_below via harness::ground_below):
// room-door tiles are the AUTHORED content-row position, which in room 0 sits well above the true
// floor (the player free-falls from spawn to the floor — see stage 1 below); the game itself resolves
// a door's landing spot down to the real floor, so the reachability proof must too. Safe no-op for
// already-floor-adjacent doors (rooms 1/2, and every other room's door): ground_below just returns
// the same content-row-adjacent tile in that case.
static bool door_reachable(const LevelData&, const RSet& R, const harness::Grid& g, int tx, int ty){
    auto gb = harness::ground_below(g, tx, ty);
    for(int lx = gb.first-harness::PW+1; lx <= gb.first; ++lx)
        if(R.count({lx, gb.second-1})) return true;
    return false;
}
static bool reaches_forward_exit(const LevelData& L, const RSet& R, const harness::Grid& g){
    for(int i = 0; i < L.room_door_count; ++i)
        if(door_reachable(L, R, g, L.room_doors[i].tx, L.room_doors[i].ty)) return true;
    if(L.has_cage && stands_at(L, R, L.cage_tx, L.cage_ty)) return true;
    if(L.has_exit && stands_at(L, R, L.exit_tx, L.exit_ty)) return true;
    return false;
}

static int d2_tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }

// ===========================================================================
// Structural invariants
// ===========================================================================
TEST(d2_dungeon_table){
    CHECK_EQ(DUNGEON2_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON2_DUNGEON.start_room, 0);
    CHECK(DUNGEON2_DUNGEON.rooms[0] == &DUNGEON2_ROOM0_DATA);
    CHECK(DUNGEON2_DUNGEON.rooms[1] == &DUNGEON2_ROOM1_DATA);
    CHECK(DUNGEON2_DUNGEON.rooms[2] == &DUNGEON2_ROOM2_DATA);
}

// Room 1 is the boss arena: >= 30 wide, has Slagshell (D2_DEF), NO cage / NO exit.
// Room 0 is the entry with the Fire shrine and NO cage / NO exit.
// Room 2 is the spronk chamber: has the cage + the exit. No other room has a boss.
TEST(d2_room1_is_boss_arena){
    const LevelData& L = DUNGEON2_ROOM1_DATA;
    REQUIRE(L.boss != nullptr);
    CHECK(L.boss == &D2_DEF);              // the canonical D2 boss symbol
    CHECK_EQ((int)L.boss->phase_count, 2);
    CHECK(!L.has_cage);
    CHECK(!L.has_exit);
    CHECK(L.w >= 30);
}
TEST(d2_only_room1_has_boss){
    CHECK(DUNGEON2_ROOM0_DATA.boss == nullptr);
    CHECK(DUNGEON2_ROOM2_DATA.boss == nullptr);
}
TEST(d2_room0_entry_no_cage_has_fire){
    const LevelData& L = DUNGEON2_ROOM0_DATA;
    CHECK(!L.has_cage);
    CHECK(!L.has_exit);
    CHECK_EQ(L.pickup_count, 1);
    CHECK(L.pickups[0].ability == Ability::Fire);
}
TEST(d2_room2_has_cage_and_exit){
    const LevelData& L = DUNGEON2_ROOM2_DATA;
    CHECK(L.has_cage);
    CHECK(L.has_exit);
    // Grounded: solid tile directly below the cage + exit (they don't float).
    CHECK(d2_tile(L, L.cage_tx, L.cage_ty+1) == 1);
    CHECK(d2_tile(L, L.exit_tx, L.exit_ty+1) == 1);
}

// Room 1 (boss arena) has NO magic crystal: magic is regained by BLOCKING Slagshell's bolts with Fire
// (block_with_spell -> magic.heal), with death-restart as the ultimate refill. The crystal was removed
// in QA in favour of the block-charge economy.
TEST(d2_room1_no_magic_crystal){
    CHECK_EQ(DUNGEON2_ROOM1_DATA.magic_crystal_count, 0);
}

// D2 room 0 has the lava row (the puzzle hallmark of Ember Caverns).
TEST(d2_room0_has_lava){
    const LevelData& L = DUNGEON2_ROOM0_DATA;
    bool lava=false;
    for(int i=0;i<L.w*L.h;++i) if((TileKind)L.tiles[i]==TileKind::Lava) lava=true;
    CHECK(lava);
}

// D2 room 0 retains the fire-immune enemy (from the original puzzle).
TEST(d2_room0_has_fire_immune_enemy){
    const LevelData& L = DUNGEON2_ROOM0_DATA;
    int immune = 0;
    for(int i=0;i<L.enemy_count;++i) if(L.enemies[i].param2 & 1) ++immune;
    CHECK_EQ(immune, 1);
}

// Room-doors resolve to real targets + matching entrance ids; exactly one hub-exit 'Q' in room 0.
// Pins the 0 <-> 1 <-> 2 graph (+ the hub return in room 0). Target-resolution is also covered
// generically by test_all_dungeons.cpp; this test additionally pins the D2-specific graph edges.
TEST(d2_room_doors_resolve){
    for(int r = 0; r < D2_N; ++r){
        const LevelData& L = *D2_ROOMS[r];
        for(int i = 0; i < L.room_door_count; ++i){
            const RoomDoorSpawn& d = L.room_doors[i];
            if(d.target_room < 0) continue;   // hub-exit 'Q'
            CHECK(d.target_room >= 0 && d.target_room < D2_N);
            const LevelData& T = *D2_ROOMS[d.target_room];
            bool found=false; for(int e=0;e<T.entrance_count;++e) if(T.entrances[e].id==d.target_entrance) found=true;
            CHECK(found);
        }
    }
    auto has_door=[&](const LevelData& L,int tr,int te){
        for(int i=0;i<L.room_door_count;++i)
            if(L.room_doors[i].target_room==tr && L.room_doors[i].target_entrance==te) return true;
        return false;
    };
    CHECK(has_door(DUNGEON2_ROOM0_DATA,1,0));   // entry -> boss arena
    CHECK(has_door(DUNGEON2_ROOM1_DATA,2,0));   // boss arena -> spronk
    CHECK(has_door(DUNGEON2_ROOM1_DATA,0,1));   // boss arena -> back to entry
    CHECK(has_door(DUNGEON2_ROOM2_DATA,1,1));   // spronk -> back to boss arena
    // exactly one hub-exit door, in room 0
    int hub=0; for(int r=0;r<D2_N;++r){ const LevelData& L=*D2_ROOMS[r];
        for(int i=0;i<L.room_door_count;++i) if(L.room_doors[i].target_room==-1) ++hub; }
    CHECK_EQ(hub, 1);
}

// ===========================================================================
// NO-SOFT-LOCK INVARIANTS (first-class). Each verified to FAIL on a broken layout.
// ===========================================================================

// 1. Room 0: the hub-return door 'Q' sits on the SPAWN content row (elevated well above the true
//    floor — the player free-falls from spawn to the floor, same as the door). harness::ground_below
//    snaps a floating content tile down to its real standable floor position (mirrors
//    scene_dungeon.cpp's floor_row_below, which is how the game itself resolves a door's landing
//    spot). Break test: wall off the floor under Q -> ground_below finds no support / the snapped
//    tile becomes unreachable -> RED.
TEST(d2_room0_hub_door_reachable_from_spawn){
    const LevelData& L = DUNGEON2_ROOM0_DATA;
    harness::WorldModel wm{};
    harness::Grid g = harness::build_grid(L, wm);
    RSet R = harness::reachable_from(L, wm, L.spawn_tx, L.spawn_ty);
    bool hub_door = false;
    for(int i=0;i<L.room_door_count;++i){
        if(L.room_doors[i].target_room != -1) continue;
        if(door_reachable(L, R, g, L.room_doors[i].tx, L.room_doors[i].ty)) hub_door = true;
    }
    CHECK(hub_door);
    // onward door to room 1 exists in room 0 (structural check — reachability proven below by the
    // staged puzzle-solvability chain, which supersedes the old exemption).
    bool onward_exists = false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==1) onward_exists=true;
    CHECK(onward_exists);
    std::printf("  [room0] hub-door=%s onward-door-exists=%s\n", hub_door?"reach":"NO", onward_exists?"yes":"NO");
}

// 2. Room 1 (boss arena): from the room-1 entrance the arena floor is traversable and the onward door
//    to room 2 is reachable (from entrance id 0). Break test: wall off the onward door -> RED.
TEST(d2_room1_onward_door_reachable){
    const LevelData& L = DUNGEON2_ROOM1_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable(L, wm);
    bool onward = false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==2 && stands_at(L, R, L.room_doors[i].tx, L.room_doors[i].ty)) onward=true;
    CHECK(onward);
    std::printf("  [room1] onward-door(to room2)=%s\n", onward?"reach":"NO");
}

// 3. Room 2 (spronk): from the room-2 entrance BOTH the caged spronk AND the exit are reachable
//    with ONLY bolt + double-jump (no Fire/Ice/etc.).
//    Break test: float/wall the cage or exit -> RED.
TEST(d2_room2_spronk_and_exit_reachable){
    const LevelData& L = DUNGEON2_ROOM2_DATA;
    harness::WorldModel wm{};
    RSet R = harness::reachable(L, wm);
    bool c = stands_at(L, R, L.cage_tx, L.cage_ty);
    bool e = stands_at(L, R, L.exit_tx, L.exit_ty);
    CHECK(c); CHECK(e);
    std::printf("  [room2] cage=%s exit=%s\n", c?"reach":"NO", e?"reach":"NO");
}

// 4. No one-way traps: from each room's entrance the player can reach a forward exit
//    (a door/cage/exit). With bolt+double-jump every room must be solvable AND escapable. Room 0's
//    "forward exit" from spawn is the hub-return 'Q' (ground-snapped — see reaches_forward_exit):
//    the onward door there is legitimately puzzle-gated (proven by the staged chain below), but the
//    hub retreat must always be free, so this still catches a genuine no-escape soft-lock.
//    Break test: drop floor under entrance -> RED.
TEST(d2_no_one_way_traps){
    for(int r = 0; r < D2_N; ++r){
        const LevelData& L = *D2_ROOMS[r];
        harness::WorldModel wm{};
        harness::Grid g = harness::build_grid(L, wm);
        RSet R = (r==0) ? harness::reachable_from(L, wm, L.spawn_tx, L.spawn_ty) : harness::reachable(L, wm);
        bool fwd = reaches_forward_exit(L, R, g);
        CHECK(fwd);
        std::printf("  [oneway] room %d -> forward exit reachable = %s\n", r, fwd?"yes":"NO");
    }
}

// ===========================================================================
// ROOM 0 — STAGED PUZZLE-SOLVABILITY PROOF (I4, D2 decision). Removes the exemption this test used
// to document (the old d2_room0_hub_door_reachable_from_spawn comment read: "The Fire shrine and the
// onward door are puzzle-gated ... their reachability is guaranteed by puzzle design, not free-
// traversal flood-fill"). Ember Caverns room 0 is three wall-pillar sections (compiled-solid 2-wide
// columns at the plate/button/brazier-group TARGET columns) chained: plate -> button -> Fire shrine
// -> Vine+Ice gates -> brazier group -> onward door. Every trigger->target pair below is read from
// the room's COMPILED data (DUNGEON2_ROOM0_DATA.plates/buttons/brazier_groups/gates), not hardcoded
// coordinates, so a re-layout keeps this test honest. Each stage proves BOTH directions: the wall
// stays sealed (climb_max, the edge apex jump) before its trigger fires, and the next landmark
// becomes reliably reachable (CLIMB_RELIABLE) once it does.
// ===========================================================================
namespace {
struct D2Puzzle {
    const LevelData& L;
    int vine_idx = -1, ice_idx = -1;   // Vine/Ice obstacle-gate indices (both cleared by Fire)
    int onward_tx = -1, onward_ty = -1;
    D2Puzzle() : L(DUNGEON2_ROOM0_DATA) {
        for(int i=0;i<L.gate_count;++i){
            if(L.gates[i].type==GateType::Vine) vine_idx=i;
            if(L.gates[i].type==GateType::Ice)  ice_idx=i;
        }
        for(int i=0;i<L.room_door_count;++i)
            if(L.room_doors[i].target_room==1){ onward_tx=L.room_doors[i].tx; onward_ty=L.room_doors[i].ty; }
    }
};
} // namespace

// Stage 1: with NOTHING open, the puzzle's first trigger (the heavy... no, the FIRST plate) must be
// reachable from spawn — it sits in the entry section, before any wall pillar.
TEST(d2_room0_stage1_plate_reachable_from_spawn){
    D2Puzzle P;
    const LevelData& L = P.L;
    REQUIRE(L.plate_count >= 1);
    harness::WorldModel wm{};
    RSet R = harness::reachable_from(L, wm, L.spawn_tx, L.spawn_ty);
    bool ok = stands_at(L, R, L.plates[0].tx, L.plates[0].ty);
    CHECK(ok);
    std::printf("  [d2-stage1] plate(%d,%d) reachable from spawn = %s\n", L.plates[0].tx, L.plates[0].ty, ok?"yes":"NO");
}

// Stage 2: the plate's target column gates the button. Closed -> unreachable even at the edge apex
// jump (CLIMB_MAX). Opened (open_columns={plate.target_tx}) -> the button is reliably reachable.
TEST(d2_room0_stage2_button_gated_by_plate_column){
    D2Puzzle P;
    const LevelData& L = P.L;
    REQUIRE(L.button_count >= 1);
    const int target = L.plates[0].target_tx;

    harness::WorldModel closed{}; closed.climb_max = true;
    RSet seen_closed = harness::reachable_from(L, closed, L.spawn_tx, L.spawn_ty);
    bool reachable_closed = stands_at(L, seen_closed, L.buttons[0].tx, L.buttons[0].ty);
    CHECK(!reachable_closed);

    harness::WorldModel open{}; open.open_columns.insert(target);
    RSet seen_open = harness::reachable_from(L, open, L.spawn_tx, L.spawn_ty);
    bool reachable_open = stands_at(L, seen_open, L.buttons[0].tx, L.buttons[0].ty);
    CHECK(reachable_open);
    std::printf("  [d2-stage2] button(%d,%d) via plate-col %d: closed=%s open=%s\n",
                L.buttons[0].tx, L.buttons[0].ty, target,
                reachable_closed?"REACHABLE(bad)":"gated", reachable_open?"reachable":"STILL-GATED(bad)");
}

// Stage 3: the button's target column gates the Fire shrine. Closed (plate-col open only) -> shrine
// unreachable even at CLIMB_MAX. Opened (both plate+button columns) -> shrine reliably reachable.
TEST(d2_room0_stage3_shrine_gated_by_button_column){
    D2Puzzle P;
    const LevelData& L = P.L;
    REQUIRE(L.pickup_count >= 1);
    const int plate_target = L.plates[0].target_tx, button_target = L.buttons[0].target_tx;

    harness::WorldModel closed{}; closed.open_columns.insert(plate_target); closed.climb_max = true;
    RSet seen_closed = harness::reachable_from(L, closed, L.spawn_tx, L.spawn_ty);
    bool reachable_closed = stands_at(L, seen_closed, L.pickups[0].tx, L.pickups[0].ty);
    CHECK(!reachable_closed);

    harness::WorldModel open{}; open.open_columns.insert(plate_target); open.open_columns.insert(button_target);
    RSet seen_open = harness::reachable_from(L, open, L.spawn_tx, L.spawn_ty);
    bool reachable_open = stands_at(L, seen_open, L.pickups[0].tx, L.pickups[0].ty);
    CHECK(reachable_open);
    std::printf("  [d2-stage3] Fire shrine(%d,%d) via button-col %d: closed=%s open=%s\n",
                L.pickups[0].tx, L.pickups[0].ty, button_target,
                reachable_closed?"REACHABLE(bad)":"gated", reachable_open?"reachable":"STILL-GATED(bad)");
}

// Stage 4: past the shrine sit the Vine + Ice obstacle gates (both cleared by Fire, just earned) —
// beyond them, every brazier of the group. Gates CLOSED (only the two wall columns open) -> no
// brazier tile reachable even at CLIMB_MAX. Gates OPENED (Fire owned at this point in the run, so
// open_gates is legitimate — see harness::WorldModel::open_gates doc) -> every brazier of the group
// reliably reachable. Scope note: the harness has no spell-inventory model, so this proves the
// brazier TILE is reachable, not that it can actually be lit (that needs Fire, which the player has
// by construction: the shrine sits strictly before the Vine/Ice gates on this path).
TEST(d2_room0_stage4_every_brazier_reachable_once_fire_clears_gates){
    D2Puzzle P;
    const LevelData& L = P.L;
    REQUIRE(P.vine_idx >= 0); REQUIRE(P.ice_idx >= 0);
    REQUIRE(L.brazier_count >= 1);
    const int plate_target = L.plates[0].target_tx, button_target = L.buttons[0].target_tx;

    harness::WorldModel closed{};
    closed.open_columns.insert(plate_target); closed.open_columns.insert(button_target);
    closed.climb_max = true;
    RSet seen_closed = harness::reachable_from(L, closed, L.spawn_tx, L.spawn_ty);

    harness::WorldModel open{};
    open.open_columns.insert(plate_target); open.open_columns.insert(button_target);
    open.open_gates.insert(P.vine_idx);   // Fire clears Vine — already earned at the shrine (stage 3)
    open.open_gates.insert(P.ice_idx);    // Fire clears Ice  — same
    RSet seen_open = harness::reachable_from(L, open, L.spawn_tx, L.spawn_ty);

    int checked = 0;
    for(int i=0;i<L.brazier_count;++i){
        bool rc = stands_at(L, seen_closed, L.braziers[i].tx, L.braziers[i].ty);
        bool ro = stands_at(L, seen_open,   L.braziers[i].tx, L.braziers[i].ty);
        CHECK(!rc);
        CHECK(ro);
        std::printf("  [d2-stage4] brazier[%d](%d,%d): gates-closed=%s gates-open=%s\n",
                    i, L.braziers[i].tx, L.braziers[i].ty, rc?"REACHABLE(bad)":"gated", ro?"reachable":"STILL-GATED(bad)");
        ++checked;
    }
    CHECK(checked == L.brazier_count);   // every brazier of the group was checked, not a subset
}

// Stage 5 (final): the brazier group's target column gates the onward door to room 1. With every
// prior column/gate open but the brazier group's wall CLOSED, the door is unreachable even at
// CLIMB_MAX. Lighting the group (all braziers -> open_columns += group.target_tx) makes the onward
// door reliably reachable — the last link in the chain from spawn to room 1.
TEST(d2_room0_stage5_onward_door_gated_by_brazier_group){
    D2Puzzle P;
    const LevelData& L = P.L;
    REQUIRE(L.brazier_group_count >= 1);
    REQUIRE(P.onward_tx >= 0);
    const int plate_target = L.plates[0].target_tx, button_target = L.buttons[0].target_tx;
    const int group_target = L.brazier_groups[0].target_tx;

    harness::WorldModel closed{};
    closed.open_columns.insert(plate_target); closed.open_columns.insert(button_target);
    closed.open_gates.insert(P.vine_idx); closed.open_gates.insert(P.ice_idx);
    closed.climb_max = true;
    RSet seen_closed = harness::reachable_from(L, closed, L.spawn_tx, L.spawn_ty);
    bool reachable_closed = stands_at(L, seen_closed, P.onward_tx, P.onward_ty);
    CHECK(!reachable_closed);

    harness::WorldModel open{};
    open.open_columns.insert(plate_target); open.open_columns.insert(button_target); open.open_columns.insert(group_target);
    open.open_gates.insert(P.vine_idx); open.open_gates.insert(P.ice_idx);
    RSet seen_open = harness::reachable_from(L, open, L.spawn_tx, L.spawn_ty);
    bool reachable_open = stands_at(L, seen_open, P.onward_tx, P.onward_ty);
    CHECK(reachable_open);
    std::printf("  [d2-stage5] onward door(%d,%d) via brazier-group-col %d: closed=%s open=%s\n",
                P.onward_tx, P.onward_ty, group_target,
                reachable_closed?"REACHABLE(bad)":"gated", reachable_open?"reachable":"STILL-GATED(bad)");
}
