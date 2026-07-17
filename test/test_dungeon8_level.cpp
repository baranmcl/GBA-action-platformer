#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon8_room0.h"
#include "game/levels/dungeon8_room1.h"
#include "game/levels/dungeon8_room2.h"
#include <vector>
#include <algorithm>
#include <functional>
#include <set>
#include <utility>
#include <cstdio>
using namespace logic;

// Gloom Spire (Dungeon 8) — the Light-spell timed-reveal ascent dungeon (M10). First-class no-soft-lock
// invariants that each FAIL on a deliberately-broken layout (verified during authoring). Reachability now
// runs on the shared test/level_harness.h model instead of a private flood-fill fork; the harness's
// dual-threshold double-jump (CLIMB_RELIABLE=5 / CLIMB_MAX=7) is exactly the M10 QA fix this dungeon
// pioneered (the "light platform is not reachable" incident — a single CLIMB=7 called a 7-up first
// platform "reachable" when ~7 is only the pixel-perfect apex edge).
//
// M10 kit -> WorldModel mapping:
//   * BASE movement  = hidden platforms NON-solid + DarkVeil CLOSED (harness fills every non-open gate) +
//     cracked SOLID. Must-NOT-bypass checks flood BASE at CLIMB_MAX (WorldModel::climb_max=true): even an
//     edge double-jump must not cross a Light-less gap (>7).
//   * LIT (Light used) = hidden platforms revealed (hidden_platforms_shown) + every DarkVeil gate opened
//     (open_gates by index). Must-reach checks flood LIT at CLIMB_RELIABLE (climb_max=false): every
//     revealed step is <=5, i.e. reliably climbable by the real player, not the apex edge.
// glide extends HORIZONTAL distance and is not modeled, so all D8 Light-climbs are VERTICAL (hidden-only
// footholds, no grapple anchor) — the only shape this model can soundly prove requires Light.

static const LevelData* const D8_ROOMS[] = {
    &DUNGEON8_ROOM0_DATA, &DUNGEON8_ROOM1_DATA, &DUNGEON8_ROOM2_DATA };
static constexpr int D8_N = 3;

using RSet = std::set<std::pair<int,int>>;

// BASE movement (hidden non-solid, DarkVeil closed, cracked solid). `edge` picks the flood reach:
// true -> CLIMB_MAX (must-NOT-bypass proof); false -> CLIMB_RELIABLE (must-reach proof).
static harness::WorldModel base_wm(bool edge){
    harness::WorldModel wm{};
    wm.climb_max = edge;
    return wm;
}
// LIT: Light revealed every hidden platform + opened every DarkVeil gate. Flooded reliably (climb_max=false).
static harness::WorldModel lit_wm(const LevelData& L){
    harness::WorldModel wm{};
    wm.hidden_platforms_shown = true;
    for(int i = 0; i < L.gate_count; ++i)
        if(L.gates[i].type == GateType::DarkVeil) wm.open_gates.insert(i);
    return wm;
}

static bool stands_at(const LevelData& L, const RSet& R, int tx, int ty){
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

// ===========================================================================
// Standard structural invariants (solid-border + min-size + settle are covered generically by
// test_all_dungeons.cpp; these are the D8-specific ones)
// ===========================================================================
static bool d8_solid(const LevelData& L, int tx, int ty){
    if(tx<0||ty<0||tx>=L.w||ty>=L.h) return true;
    return (int)L.tiles[ty*L.w + tx] == (int)TileKind::Solid;
}

TEST(d8_dungeon_table){
    CHECK_EQ(DUNGEON8_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON8_DUNGEON.start_room, 0);
    CHECK(DUNGEON8_DUNGEON.rooms[0] == &DUNGEON8_ROOM0_DATA);
    CHECK(DUNGEON8_DUNGEON.rooms[2] == &DUNGEON8_ROOM2_DATA);
}

TEST(d8_one_light_shrine){
    int shrines = 0; bool light = false;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        for(int i = 0; i < L.pickup_count; ++i){ ++shrines; if(L.pickups[i].ability==Ability::Light) light=true; }
    }
    CHECK_EQ(shrines, 1); CHECK(light);
}

TEST(d8_one_spronk_one_exit_grounded){
    // D8 is a vertical ASCENT — the spronk + exit sit on a top ledge, not the content row. So unlike the
    // flat dungeons we do NOT require cage_ty==18; we require they are GROUNDED (a solid tile directly
    // below the sprite tile) so they don't float, and that there is exactly one of each.
    int cages=0, exits=0;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        if(L.has_cage){ ++cages; CHECK(d8_solid(L,L.cage_tx,L.cage_ty+1)); }
        if(L.has_exit){ ++exits; CHECK(d8_solid(L,L.exit_tx,L.exit_ty+1)); }
    }
    CHECK_EQ(cages,1); CHECK_EQ(exits,1);
}

TEST(d8_room_doors_resolve_and_two_way){
    // Target-resolution is also covered generically by test_all_dungeons.cpp; this test additionally pins
    // the D8-specific two-way wiring (co-located return entrances + the explicit hub<->branch topology).
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        for(int i = 0; i < L.room_door_count; ++i){
            const RoomDoorSpawn& d = L.room_doors[i];
            if(d.target_room < 0) continue;   // hub-exit 'Q'
            CHECK(d.target_room>=0 && d.target_room<D8_N);
            const LevelData& T = *D8_ROOMS[d.target_room];
            bool found=false; for(int e=0;e<T.entrance_count;++e) if(T.entrances[e].id==d.target_entrance) found=true;
            CHECK(found);
            // Co-located return entrance: a same-row entrance within 1..2 tiles of the door. (D8's doors
            // are all on the open base floor — no across-the-gate exception needed.)
            bool near=false;
            for(int e=0;e<L.entrance_count;++e){
                const EntranceSpawn& n=L.entrances[e];
                if(n.ty!=d.ty) continue;
                int dx=n.tx-d.tx; if(dx<0) dx=-dx;
                if(dx>=1 && dx<=2) near=true;
            }
            CHECK(near);
        }
    }
    auto has_door=[&](const LevelData& L,int tr,int te){
        for(int i=0;i<L.room_door_count;++i)
            if(L.room_doors[i].target_room==tr && L.room_doors[i].target_entrance==te) return true;
        return false;
    };
    auto has_ent=[&](const LevelData& L,int id){
        for(int i=0;i<L.entrance_count;++i) if(L.entrances[i].id==id) return true;
        return false;
    };
    // Room 0 hubs to 1 and 2; each branch returns to a distinct room-0 entrance (id 1, id 2).
    CHECK(has_door(DUNGEON8_ROOM0_DATA,1,0)); CHECK(has_door(DUNGEON8_ROOM0_DATA,2,0));
    CHECK(has_door(DUNGEON8_ROOM1_DATA,0,1)); CHECK(has_ent(DUNGEON8_ROOM1_DATA,0));
    CHECK(has_door(DUNGEON8_ROOM2_DATA,0,2)); CHECK(has_ent(DUNGEON8_ROOM2_DATA,0));
    CHECK(has_ent(DUNGEON8_ROOM0_DATA,1)); CHECK(has_ent(DUNGEON8_ROOM0_DATA,2));
}

TEST(d8_has_latched_shortcut){
    // >=1 CrackedFloor gate carries a latch_id >= 0 (the SRAM-persisted shortcut, room 1).
    int latched = 0;
    for(int r = 0; r < D8_N; ++r){ const LevelData& L = *D8_ROOMS[r];
        for(int i=0;i<L.gate_count;++i)
            if(L.gates[i].type==GateType::CrackedFloor && L.gates[i].latch_id>=0) ++latched; }
    CHECK(latched >= 1);
}

TEST(d8_uses_darkveil_and_hidden_and_crystals){
    // Non-vacuity: the dungeon actually uses the M10 kit — DarkVeil gates, hidden platforms, crystals.
    int dv=0, hp=0, mc=0;
    for(int r = 0; r < D8_N; ++r){ const LevelData& L = *D8_ROOMS[r];
        for(int i=0;i<L.gate_count;++i) if(L.gates[i].type==GateType::DarkVeil) ++dv;
        hp += L.hidden_platform_count;
        mc += L.magic_crystal_count;
    }
    CHECK(dv >= 1); CHECK(hp >= 1); CHECK(mc >= 1);
}

// Room 1 is the heart-container REWARD room (M10 QA: "give Room 1 a purpose"). It holds exactly one heart
// container, GROUNDED (a solid tile directly below it), and SEALED behind the Light puzzle: from the room-1
// entrance it is BASE-unreachable (hidden platforms non-solid + DarkVeil closed) but LIT-reachable (reveal
// + open). So the heart is an OPTIONAL permanent-max-HP reward for using Light. Break tests: float the
// heart (no solid below) -> RED; make the climb static '#' so base reaches it -> base-reachable -> RED;
// wall it off so lit can't reach -> lit-miss -> RED.
TEST(d8_room1_has_heart_container){
    const LevelData& L = DUNGEON8_ROOM1_DATA;
    CHECK_EQ(L.heart_container_count, 1);
    const HeartContainerSpawn& hc = L.heart_containers[0];
    CHECK(hc.id >= 0);                        // heart-container id (global uniqueness enforced by the
                                              // Phase-1 validator; no cross-dungeon serial pin here)
    CHECK(d8_solid(L, hc.tx, hc.ty+1));       // grounded (the engine grounds it onto the ledge below)

    bool base_h = harness::reaches(L, base_wm(/*edge*/true), hc.tx, hc.ty);   // edge double-jump can't bypass
    CHECK(!base_h);                           // sealed: NOT reachable before Light (gap >7)

    bool lit_h = harness::reaches(L, lit_wm(L), hc.tx, hc.ty);                // every revealed step <=5
    CHECK(lit_h);                             // RELIABLY reachable once Light reveals the climb

    std::printf("  [room1-heart] H(%d,%d) id=%d grounded=yes base=%s lit=%s\n",
                hc.tx, hc.ty, hc.id, base_h?"reach(bad)":"sealed", lit_h?"reach":"MISS(bad)");
}

// ===========================================================================
// NO-SOFT-LOCK / REQUIRES-LIGHT INVARIANTS (first-class). Each verified to FAIL on a broken layout.
// ===========================================================================

// 1. The Light 'F' shrine is BASE-reachable (hidden platforms NON-solid, DarkVeil CLOSED) from the
//    dungeon-entry spawn — the player can EARN Light before any Light-gated beat. Break test: move the
//    shrine behind the DarkVeil gate (or onto a hidden platform) -> base-unreachable -> RED.
TEST(d8_light_shrine_reachable_without_light){
    int checked = 0;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        for(int i=0;i<L.pickup_count;++i){
            if(L.pickups[i].ability != Ability::Light) continue;
            // BASE, reliably walkable (CLIMB_RELIABLE) from the raw dungeon-entry spawn.
            RSet R = harness::reachable_from(L, base_wm(/*edge*/false), L.spawn_tx, L.spawn_ty);
            bool reach = stands_at(L, R, L.pickups[i].tx, L.pickups[i].ty);
            CHECK(reach);
            std::printf("  [shrine] room %d F(%d,%d) base-reachable=%s\n",
                        r, L.pickups[i].tx, L.pickups[i].ty, reach?"yes":"NO");
            ++checked;
        }
    }
    CHECK_EQ(checked, 1);   // exactly the one Light shrine
}

// 2. The spronk/exit REQUIRES Light. With BASE movement (hidden platforms NON-solid + DarkVeil CLOSED)
//    the cage C and exit E are NOT reachable from the room entrance even at the edge reach (CLIMB_MAX);
//    once hidden platforms are revealed AND DarkVeil opened (a Light cast) they BECOME reliably reachable
//    (CLIMB_RELIABLE). Proves Light is genuinely required. Break test: make the ascent platforms ordinary
//    solid '#' (so base reaches the top) -> base goes REACHABLE -> RED.
TEST(d8_spronk_requires_light){
    const LevelData& L = DUNGEON8_ROOM2_DATA;
    CHECK(L.has_cage); CHECK(L.has_exit);

    // (a) BASE at the edge reach (CLIMB_MAX=7): every Light-less static gap up the ascent is >7.
    RSet base = harness::reachable(L, base_wm(/*edge*/true));
    bool base_c = stands_at(L, base, L.cage_tx, L.cage_ty);
    bool base_e = stands_at(L, base, L.exit_tx, L.exit_ty);
    CHECK(!base_c); CHECK(!base_e);

    // (b) Light used: reveal + open, flooded at CLIMB_RELIABLE=5 (every revealed step <=5 -> reliably
    //     climbable by the REAL player, not the apex-perfect pixel edge — the M10 QA fix).
    RSet lit = harness::reachable(L, lit_wm(L));
    bool lit_c = stands_at(L, lit, L.cage_tx, L.cage_ty);
    bool lit_e = stands_at(L, lit, L.exit_tx, L.exit_ty);
    CHECK(lit_c); CHECK(lit_e);

    std::printf("  [requires-light] C(%d,%d) E(%d,%d): base=(%s,%s) lit=(%s,%s)\n",
                L.cage_tx,L.cage_ty,L.exit_tx,L.exit_ty,
                base_c?"reach(bad)":"gated", base_e?"reach(bad)":"gated",
                lit_c?"reach":"MISS(bad)", lit_e?"reach":"MISS(bad)");
}

// 3. A magic crystal '$' is reachable (the guaranteed combat-free full refill) BEFORE each hidden-platform
//    region — no magic soft-lock. For each distinct hidden-platform ROW R, require a '$' crystal that
//    (i) sits on genuinely SOLID ground (NOT on a hidden platform), (ii) is reachable in the LIT grid, and
//    (iii) sits at a row >= R (you reach the refuel before climbing up into that region). An enemy does NOT
//    count. Break test: delete the floor/ledge crystal -> a region loses its before-it refuel -> RED.
TEST(d8_magic_crystal_before_each_light_beat){
    int regions_checked = 0;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        if(L.hidden_platform_count == 0) continue;
        RSet lit = harness::reachable(L, lit_wm(L));   // crystal must be reliably reachable

        // a crystal sits on PERSISTENT solid ground (a rest ledge / floor) — NOT on a fading hidden
        // platform — iff there is solid within 2 rows below it in the BASE grid (hidden platforms
        // non-solid): a real rest point, distinguishing a floor/ledge crystal from one perched mid-air
        // on a hidden step.
        harness::Grid base = harness::build_grid(L, base_wm(/*edge*/false));
        auto crystal_on_solid = [&](const MagicCrystalSpawn& c)->bool{
            for(int dy=1; dy<=2; ++dy)
                if(base.blk(c.tx, c.ty+dy) || base.blk(c.tx+1, c.ty+dy)) return true;
            return false;
        };
        auto crystal_reachable = [&](const MagicCrystalSpawn& c)->bool{
            return stands_at(L, lit, c.tx, c.ty);
        };

        // distinct hidden-platform rows = the climb's "beats", sorted BOTTOM-UP (largest ty first, since
        // ty grows downward). The refuel for each beat must be a DISTINCT crystal on the rest ledge LEADING
        // INTO that beat — i.e. its row lies in [R, R_prev) where R_prev is the previous (lower) beat's row
        // (or the room bottom for the first beat). This forbids a single floor crystal from "covering" every
        // beat: a higher beat demands a crystal physically near it, so removing any rest-ledge crystal -> RED.
        std::vector<int> rows;
        for(int i=0;i<L.hidden_platform_count;++i){
            int R = L.hidden_platforms[i].ty; bool seen_row=false;
            for(int v : rows) if(v==R) seen_row=true;
            if(!seen_row) rows.push_back(R);
        }
        std::sort(rows.begin(), rows.end(), std::greater<int>());   // bottom-up
        int prev = L.h;   // band ceiling for the first (lowest) beat = the room bottom
        for(int R : rows){
            bool ok = false;
            for(int i=0;i<L.magic_crystal_count;++i){
                const MagicCrystalSpawn& c = L.magic_crystals[i];
                if(c.ty >= R && c.ty < prev && crystal_on_solid(c) && crystal_reachable(c)) ok = true;
            }
            CHECK(ok);
            std::printf("  [crystal-beat] room %d h-region row %d (band [%d,%d)) -> refuel before it: %s\n",
                        r, R, R, prev, ok?"yes":"NO");
            ++regions_checked;
            prev = R;
        }
    }
    CHECK(regions_checked >= 1);
}

// 4. No pit traps / no one-way traps: flood from the entrance on the Light-used frontier (reveal + open)
//    and require a forward exit reachable. (With Light, the room must be solvable AND escapable.)
TEST(d8_no_one_way_traps){
    int checked = 0;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        RSet lit = harness::reachable(L, lit_wm(L));   // real player must escape (CLIMB_RELIABLE)
        bool fwd = reaches_forward_exit(L, lit);
        CHECK(fwd);
        std::printf("  [oneway] room %d -> forward exit reachable (lit) = %s\n", r, fwd?"yes":"NO");
        ++checked;
    }
    CHECK_EQ(checked, D8_N);
}

// 5. No pit traps under the climb: the fall-zone directly below EACH hidden-platform tile (scan down to
//    the first solid) is NON-hazard safe ground (Lava/Water/Spikes-free) — a fade-fall is a RETRY (fall
//    back to a safe ledge/floor + re-cast), not death-by-hazard. Break test: put lava under an h-climb ->
//    RED. (Verified by placing a '~' lava tile under a hidden platform during authoring.)
TEST(d8_reveal_climb_over_safe_ground){
    int checked = 0;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        harness::Grid base = harness::build_grid(L, base_wm(/*edge*/false));  // fall happens with platforms gone
        for(int i=0;i<L.hidden_platform_count;++i){
            const HiddenPlatformSpawn& hp = L.hidden_platforms[i];
            for(int dx=0; dx<hp.len; ++dx){
                int x = hp.tx+dx;
                // scan straight down from just below the platform to the first solid; assert no hazard en route.
                for(int y=hp.ty; y<L.h; ++y){
                    bool hz = base.haz(x,y);
                    CHECK(!hz);
                    if(hz) std::printf("  [climb-safe] room %d h(%d,%d) falls through hazard at (%d,%d)\n",
                                       r, hp.tx, hp.ty, x, y);
                    if(base.blk(x,y)) break;   // landed on solid ground
                }
                ++checked;
            }
        }
    }
    CHECK(checked >= 1);
}

// 6. Re-entry safety: with the room rebuilt into its load state (hidden platforms NON-solid, DarkVeil
//    CLOSED, cracked floors SOLID) the hub/branch progression stays sane — from the dungeon-entry spawn
//    AND every return entrance, the Light shrine (room 0) is base-reachable so a re-entering player can
//    always re-earn/keep progressing. (Room 0 only; rooms 1/2 are entered with Light already in hand.)
TEST(d8_room0_reentry_shrine_reachable){
    const LevelData& L = DUNGEON8_ROOM0_DATA;
    // find the Light shrine
    int fx=-1, fy=-1;
    for(int i=0;i<L.pickup_count;++i) if(L.pickups[i].ability==Ability::Light){ fx=L.pickups[i].tx; fy=L.pickups[i].ty; }
    CHECK(fx>=0);

    struct Seed { const char* name; int x, y; };
    std::vector<Seed> seeds;
    seeds.push_back({"spawn", L.spawn_tx, L.spawn_ty});
    for(int e=0;e<L.entrance_count;++e)
        seeds.push_back({"entrance", L.entrances[e].tx, L.entrances[e].ty});

    int verified = 0;
    for(const Seed& s : seeds){
        RSet R = harness::reachable_from(L, base_wm(/*edge*/false), s.x, s.y);  // reliably re-reach the shrine
        bool ok = stands_at(L, R, fx, fy);
        CHECK(ok);
        std::printf("  [reentry] from %s (%d,%d): shrine reachable = %s\n",
                    s.name, s.x, s.y, ok?"yes":"STRANDED");
        ++verified;
    }
    CHECK(verified >= 3);   // spawn + the two return entrances (id1, id2)
}
