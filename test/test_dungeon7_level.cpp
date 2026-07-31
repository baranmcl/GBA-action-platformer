#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon7_room0.h"
#include "game/levels/dungeon7_room1.h"
#include "game/levels/dungeon7_room2.h"
#include <vector>
#include <set>
#include <utility>
#include <cstdio>
using namespace logic;

// Quaking Quarry (Dungeon 7) — the Stone ground-pound descent dungeon (M8). First-class no-soft-lock
// invariants that each FAIL on a deliberately-broken layout (verified during authoring). Reachability now
// runs on the shared test/level_harness.h model instead of a private flood-fill fork.
//
// DUAL-THRESHOLD MIGRATION (I4): D7's fork used a single unsound CLIMB=6 (the D8 QA incident showed ~6-7
// is only the pixel-perfect apex edge, not a reliable reach). Each assertion is re-expressed on the
// harness's two constants:
//   * MUST-REACH content (heart/spronk/exit/door reachable, forward-exit escapable) floods at
//     CLIMB_RELIABLE=5 (WorldModel default) — an ordinary non-pixel-perfect air-jump. Every D7 must-reach
//     assertion holds at RELIABLE=5, so NONE needed the climb_max escape hatch.
//   * MUST-NOT-BYPASS gate proofs (heart requires pound; spronk requires freeze; room-1 door requires the
//     heavy switch) flood at CLIMB_MAX=7 (WorldModel::climb_max=true) — the gate must block even the edge
//     apex jump. This is STRICTER than the retired CLIMB=6 and all three still hold.
//
// Stone/gate kit -> WorldModel mapping (replaces the fork's build_grid_stone_cleared / build_grid_no_pound
// / freeze_water / close_dark_veil / open_heavy_gate helpers):
//   * base_open_wm      — static grid, cracked SOLID, boulders SOLID, DarkVeil passable (the fork's
//                         build_grid frontier for pound-landing scans; DarkVeil is opened because the
//                         no-soft-lock invariants model the player already progressing past it).
//   * stone_cleared_wm  — boulders broken + cracked floors smashed (the full Stone-pound frontier).
//   * no_pound_wm       — boulders broken but cracked floors RE-SOLIDIFIED (the heart-gate BASE proof).
//   * water_frozen      — Ice cast turns the water run into a standable bridge.
//   * open_columns      — the heavy switch's open_column(tx) clears the DarkVeil fill.
//   * loose_dropped     — the Stone pound has released the loose platforms to their rest row (set in the
//                         full-kit models). D7 room-1's loose platform DROPS onto the spike pit at the
//                         bottom floor, bridging it — the only floor-level crossing to the Stone-mezzanine
//                         heart alcove. With it modeled, the heart is RELIABLY reachable at CLIMB_RELIABLE
//                         (=5), so NO D7 must-reach assertion needed the climb_max escape hatch.

static const LevelData* const D7_ROOMS[] = {
    &DUNGEON7_ROOM0_DATA, &DUNGEON7_ROOM1_DATA, &DUNGEON7_ROOM2_DATA };
static constexpr int D7_N = 3;

using RSet = std::set<std::pair<int,int>>;

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

// The DarkVeil gate indices for a room (opened by the heavy switch; the "full kit" frontier treats them
// passable, exactly as the old fork's build_grid did — it never filled DarkVeil).
static void open_darkveil(const LevelData& L, harness::WorldModel& wm){
    for(int i = 0; i < L.gate_count; ++i)
        if(L.gates[i].type == GateType::DarkVeil) wm.open_gates.insert(i);
}
static harness::WorldModel base_open_wm(const LevelData& L){
    harness::WorldModel wm{}; open_darkveil(L, wm); return wm;
}
static harness::WorldModel stone_cleared_wm(const LevelData& L){
    harness::WorldModel wm{}; wm.boulders_broken = true; wm.cracked_floors_broken = true;
    wm.loose_dropped = true; open_darkveil(L, wm); return wm;
}
static harness::WorldModel no_pound_wm(const LevelData& L){
    harness::WorldModel wm{}; wm.boulders_broken = true; wm.loose_dropped = true; open_darkveil(L, wm); return wm;   // cracked stays SOLID
}
// Per-object load-state baseline: DarkVeil open (matches base_open_wm), but boulders/cracked-floors/
// loose-platforms are ALL at their load state except the ONE object under test (Task 2.4 review fix:
// the retired fork tested each boulder/loose-platform cleared INDIVIDUALLY against this baseline —
// the full-kit flood in stone_cleared_wm is strictly weaker and can't catch a single-object stranding).
static harness::WorldModel per_boulder_wm(const LevelData& L, int b){
    harness::WorldModel wm{}; open_darkveil(L, wm); wm.broken_boulder_idx.insert(b); return wm;
}
static harness::WorldModel per_loose_wm(const LevelData& L, int p){
    harness::WorldModel wm{}; open_darkveil(L, wm); wm.dropped_loose_idx.insert(p); return wm;
}

// --- Cracked-floor pound-chain scanners (D7-specific mechanic; NOT flood-fill, kept local). ---
static bool is_cracked(const LevelData& L, int tx, int ty){
    for(int i = 0; i < L.gate_count; ++i)
        if(L.gates[i].type == GateType::CrackedFloor && L.gates[i].tx==tx && L.gates[i].ty==ty) return true;
    return false;
}
// From a cracked floor (tx,ty): the pound smashes it and every stacked cracked tile below, landing on the
// first NON-cracked solid tile; the landing STANDABLE row is one above that solid tile (-1 if none).
static int pound_landing_row(const LevelData& L, const harness::Grid& g, int tx, int ty){
    int y = ty;
    while(y+1 < L.h){
        int below = y+1;
        if(is_cracked(L, tx, below)){ y = below; continue; }             // chain through stacked cracked floors
        if(g.solid[below*L.w + tx] && !is_cracked(L, tx, below)) return y; // land standing on row y
        if(!g.solid[below*L.w + tx]) { ++y; continue; }                   // empty -> keep falling
        return y;
    }
    return -1;
}
// The rest row a loose platform drops to (first row whose tiles-below are solid). Mirrors the fork's drop.
static int loose_rest_row(const LevelData& L, const harness::Grid& g, const LoosePlatformSpawn& lp){
    int ty = lp.ty;
    while(ty+1 < L.h){
        bool rest = false;
        for(int dx=0; dx<lp.len; ++dx) if(g.solid[(ty+1)*L.w + (lp.tx+dx)]) rest = true;
        if(rest) break; ++ty;
    }
    return ty;
}

// ===========================================================================
// Standard structural invariants (solid-border + settle covered generically by test_all_dungeons.cpp;
// these are the D7-specific ones)
// ===========================================================================
static bool d7_solid(const LevelData& L, int tx, int ty){
    if(tx<0||ty<0||tx>=L.w||ty>=L.h) return true;
    return (int)L.tiles[ty*L.w + tx] == (int)TileKind::Solid;
}

TEST(d7_dungeon_table){
    CHECK_EQ(DUNGEON7_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON7_DUNGEON.start_room, 0);
    CHECK(DUNGEON7_DUNGEON.rooms[0] == &DUNGEON7_ROOM0_DATA);
    CHECK(DUNGEON7_DUNGEON.rooms[2] == &DUNGEON7_ROOM2_DATA);
}

TEST(d7_one_stone_shrine){
    int shrines = 0; bool stone = false;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        for(int i = 0; i < L.pickup_count; ++i){ ++shrines; if(L.pickups[i].ability==Ability::Stone) stone=true; }
    }
    CHECK_EQ(shrines, 1); CHECK(stone);
}

TEST(d7_one_spronk_one_exit_grounded){
    int cages=0, exits=0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        if(L.has_cage){ ++cages; CHECK_EQ(L.cage_ty,18); CHECK(d7_solid(L,L.cage_tx,20)); }
        if(L.has_exit){ ++exits; CHECK_EQ(L.exit_ty,18); CHECK(d7_solid(L,L.exit_tx,20)); }
    }
    CHECK_EQ(cages,1); CHECK_EQ(exits,1);
}

TEST(d7_floor_content_on_row_18){
    // Floor-bound content (spawn/enemy/shrine) grounds on content row 18 (feet land on the row-20 floor).
    // NOTE: heart containers are EXEMPT — the engine places a heart via a static overlap body at its exact
    // tile (no floor-scan), so a heart may sit on a raised ledge (D7's is on a Stone-gated mezzanine). Its
    // grounding + Stone-gating is covered by the heart-alcove invariant; here we only require solid below.
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        CHECK_EQ(L.spawn_ty, 18);
        for(int i=0;i<L.enemy_count;++i)  CHECK_EQ((int)L.enemies[i].ty,18);
        for(int i=0;i<L.pickup_count;++i) CHECK_EQ((int)L.pickups[i].ty,18);
        for(int i=0;i<L.heart_container_count;++i){
            const HeartContainerSpawn& hc = L.heart_containers[i];
            CHECK(hc.ty+1 < L.h && (int)L.tiles[(hc.ty+1)*L.w + hc.tx] == (int)TileKind::Solid); // solid below
        }
    }
}

TEST(d7_room_doors_resolve_and_two_way){
    // Target-resolution is also covered generically by test_all_dungeons.cpp; this test additionally pins
    // the D7-specific two-way wiring including the DarkVeil-gate return-entrance exception.
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        for(int i = 0; i < L.room_door_count; ++i){
            const RoomDoorSpawn& d = L.room_doors[i];
            if(d.target_room < 0) continue;   // hub-exit door (Q, target_room=-1)
            CHECK(d.target_room>=0 && d.target_room<D7_N);
            const LevelData& T = *D7_ROOMS[d.target_room];
            bool found=false; for(int e=0;e<T.entrance_count;++e) if(T.entrances[e].id==d.target_entrance) found=true;
            CHECK(found);
            // Two-way return entrance. Normally co-located within 1..2 tiles. EXCEPTION for a GATE-GUARDED
            // door: a door sealed behind a 2-wide DarkVeil fill has its return entrance just BEYOND the gate
            // (a DarkVeil column strictly between it and the door).
            auto darkveil_between=[&](int a,int b){
                int lo=a<b?a:b, hi=a<b?b:a;
                for(int gi=0; gi<L.gate_count; ++gi)
                    if(L.gates[gi].type==GateType::DarkVeil && L.gates[gi].tx>lo && L.gates[gi].tx<hi) return true;
                return false;
            };
            bool near=false;
            for(int e=0;e<L.entrance_count;++e){
                const EntranceSpawn& n=L.entrances[e];
                if(n.ty!=d.ty) continue;
                int dx=n.tx-d.tx; if(dx<0) dx=-dx;
                if(dx>=1 && dx<=2) near=true;                                   // co-located (ungated door)
                else if(dx>=1 && darkveil_between(n.tx,d.tx)) near=true;        // paired across a DarkVeil gate
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
    CHECK(has_door(DUNGEON7_ROOM0_DATA,1,0)); CHECK(has_door(DUNGEON7_ROOM0_DATA,2,0));
    CHECK(has_door(DUNGEON7_ROOM1_DATA,0,1)); CHECK(has_ent(DUNGEON7_ROOM1_DATA,0));
    CHECK(has_door(DUNGEON7_ROOM2_DATA,0,2)); CHECK(has_ent(DUNGEON7_ROOM2_DATA,0));
    CHECK(has_ent(DUNGEON7_ROOM0_DATA,1)); CHECK(has_ent(DUNGEON7_ROOM0_DATA,2));
}

TEST(d7_has_latched_shortcut){
    // >=1 CrackedFloor gate carries a latch_id >= 0 (the SRAM-persisted Stone shortcut, room 1).
    int latched = 0;
    for(int r = 0; r < D7_N; ++r){ const LevelData& L = *D7_ROOMS[r];
        for(int i=0;i<L.gate_count;++i)
            if(L.gates[i].type==GateType::CrackedFloor && L.gates[i].latch_id>=0) ++latched; }
    CHECK(latched >= 1);
}

TEST(d7_every_cracked_floor_reachable_via_grid){
    // Every cracked floor's TILE is part of the static blocking grid (a walkable surface), i.e. it has
    // clear air directly above it so a pound can land on it.
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        harness::Grid g = harness::build_grid(L, base_open_wm(L));
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            CHECK(g.solid[ty*L.w+tx]);            // the cracked tile blocks (walkable)
            CHECK(!g.blk(tx, ty-1));              // clear air directly above (a pound can land here)
        }
    }
}

// ===========================================================================
// NO-SOFT-LOCK INVARIANTS (first-class). Each is verified to FAIL on a broken layout.
// ===========================================================================

// 1. Every cracked-floor drop has a forward exit reachable from the true landing tile.
TEST(d7_every_cracked_floor_drop_has_forward_exit){
    int checked = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        harness::Grid g = harness::build_grid(L, base_open_wm(L));   // cracked solid, for the landing scan
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            int land = pound_landing_row(L, g, tx, ty);
            CHECK(land >= 0);                              // the plunge ends on real ground
            if(land < 0) continue;
            // Flood from the landing on the Stone-cleared frontier; require a forward exit (RELIABLE reach).
            RSet R = harness::reachable_from(L, stone_cleared_wm(L), tx, land);
            bool ok = reaches_forward_exit(L, R);
            CHECK(ok);
            ++checked;
            std::printf("  [drop-exit] room %d k(%d,%d) lands row %d -> forward-exit %s\n",
                        r, tx, ty, land, ok?"reached":"MISSING");
        }
    }
    CHECK(checked >= 1);    // non-vacuity: we actually checked some drops
}

// 2. No pound lands in (or plunges through) a hazard.
TEST(d7_no_pound_lands_in_hazard){
    int checked = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        harness::Grid g = harness::build_grid(L, base_open_wm(L));
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            int land = pound_landing_row(L, g, tx, ty);
            for(int y=ty; y<=(land<0? L.h-1 : land+1); ++y){
                bool hz = g.haz(tx,y);
                CHECK(!hz);
                if(hz) std::printf("  [hazard] room %d k(%d,%d) plunge hits hazard at (%d,%d)\n", r,tx,ty,tx,y);
            }
            ++checked;
        }
    }
    CHECK(checked >= 1);
}

// 3. Manipulable objects (boulders, loose platforms) cannot strand the player.
TEST(d7_manipulable_objects_cannot_strand){
    int covered = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        int sx = harness::room_start_x(L), sy = harness::room_start_y(L);
        // boulders: with the Stone kit the player breaks EACH boulder individually; a forward exit stays
        // reachable from the entrance with ONLY that boulder cleared (a fresh WorldModel per iteration —
        // every other boulder/cracked-floor/loose-platform stays at load state), and no boulder tile
        // overlaps a door/cage/exit.
        for(int b=0;b<L.boulder_count;++b){
            RSet R = harness::reachable_from(L, per_boulder_wm(L, b), sx, sy);
            bool ok = reaches_forward_exit(L, R);
            CHECK(ok);
            for(int i=0;i<L.room_door_count;++i) CHECK(!(L.room_doors[i].tx==L.boulders[b].tx && L.room_doors[i].ty==L.boulders[b].ty));
            if(L.has_cage) CHECK(!(L.cage_tx==L.boulders[b].tx && L.cage_ty==L.boulders[b].ty));
            if(L.has_exit) CHECK(!(L.exit_tx==L.boulders[b].tx && L.exit_ty==L.boulders[b].ty));
            std::printf("  [boulder] room %d O(%d,%d) cleared alone -> forward-exit %s\n",
                        r, L.boulders[b].tx, L.boulders[b].ty, ok?"reached":"MISSING");
            ++covered;
        }
        // loose platforms: after dropping EACH platform individually to its rest row (a fresh WorldModel
        // per iteration — every other object stays at load state), a forward exit stays reachable AND the
        // rest tiles don't overlap a door/cage/exit (no sealing the goal).
        for(int p=0;p<L.loose_platform_count;++p){
            RSet R = harness::reachable_from(L, per_loose_wm(L, p), sx, sy);
            bool ok = reaches_forward_exit(L, R);
            CHECK(ok);
            const LoosePlatformSpawn& lp = L.loose_platforms[p];
            harness::Grid base = harness::build_grid(L, base_open_wm(L));
            int ty = loose_rest_row(L, base, lp);
            for(int dx=0; dx<lp.len; ++dx){ int x=lp.tx+dx;
                for(int i=0;i<L.room_door_count;++i) CHECK(!(L.room_doors[i].tx==x && L.room_doors[i].ty==ty));
                if(L.has_cage) CHECK(!(L.cage_tx==x && L.cage_ty==ty));
                if(L.has_exit) CHECK(!(L.exit_tx==x && L.exit_ty==ty));
            }
            std::printf("  [loose] room %d :(%d,%d,len%d) dropped alone -> rests row %d -> forward-exit %s\n",
                        r, lp.tx, lp.ty, lp.len, ty, ok?"reached":"MISSING");
            ++covered;
        }
        // heavy switches: each targets a column that a gate occupies (it OPENS a gate, never seals one).
        for(int i=0;i<L.plate_count;++i){ if(!L.plates[i].heavy) continue;
            bool targets_gate=false;
            for(int gi=0; gi<L.gate_count; ++gi)
                if(L.gates[gi].tx==L.plates[i].target_tx) targets_gate=true;
            CHECK(targets_gate);
            std::printf("  [heavy] room %d plate(%d,%d) -> opens gate col %d : %s\n",
                        r, L.plates[i].tx, L.plates[i].ty, L.plates[i].target_tx, targets_gate?"yes":"NO");
            ++covered;
        }
    }
    std::printf("  [objects] covered %d manipulable outcomes\n", covered);
    CHECK(covered >= 1);
}

// 4. No pound deposits the player in an inescapable pit. Every cracked-floor landing is either climbable
//    out OR reaches a forward exit OR is the intended terminal (the spronk).
TEST(d7_no_pound_pit_traps){
    int checked = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        harness::Grid g = harness::build_grid(L, base_open_wm(L));
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            int land = pound_landing_row(L, g, tx, ty);
            CHECK(land>=0); if(land<0) continue;
            RSet R = harness::reachable_from(L, stone_cleared_wm(L), tx, land);
            bool exit_ok = reaches_forward_exit(L, R);
            bool can_leave = R.size() > 1;   // the player can leave the landing column alone
            CHECK(exit_ok || can_leave);
            ++checked;
            if(!(exit_ok || can_leave))
                std::printf("  [pit] room %d k(%d,%d) land row %d is an INESCAPABLE pit\n", r,tx,ty,land);
        }
    }
    CHECK(checked >= 1);
}

// 5. Magic-gated beats have a magic source (enemy 'o') in the same room or an earlier (lower-index) room
//    along the path from start. Room 2's Ice water-gap is funded by room 0 and/or room 2 enemies.
TEST(d7_magic_beats_have_a_source_before_them){
    auto room_has_water = [](const LevelData& L)->bool{
        for(int i=0;i<L.w*L.h;++i) if((TileKind)L.tiles[i]==TileKind::Water) return true;
        return false;
    };
    int beats = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        if(!room_has_water(L)) continue;
        ++beats;
        bool source = false;
        for(int rr = 0; rr <= r; ++rr) if(D7_ROOMS[rr]->enemy_count >= 1) source = true;
        CHECK(source);
        std::printf("  [magic] room %d has a Water beat; magic source <= room %d: %s\n", r, r, source?"yes":"NO");
    }
    std::printf("  [magic] %d magic-gated beats checked\n", beats);
    CHECK(beats >= 1);
}

// 6. Heart-container alcoves are REACHABLE (with the Stone kit) AND RETURNABLE (grab H and LEAVE).
TEST(d7_heart_alcove_reachable_and_returnable){
    int checked = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        for(int i=0;i<L.heart_container_count;++i){
            const HeartContainerSpawn& hc = L.heart_containers[i];
            harness::WorldModel wm = stone_cleared_wm(L);
            harness::Grid g = harness::build_grid(L, wm);
            int sx = harness::room_start_x(L), sy = harness::room_start_y(L);
            // (a) reachable from the entrance.
            RSet from_entrance = harness::reachable_from(L, wm, sx, sy);
            bool reach = stands_at(L, from_entrance, hc.tx, hc.ty);
            CHECK(reach);
            // (b) returnable: flooding FROM a heart standing anchor must reach a forward exit (door).
            bool returnable = false;
            for(int lx = hc.tx-harness::PW+1; lx <= hc.tx && !returnable; ++lx){
                for(int fy = hc.ty+1; fy >= hc.ty && !returnable; --fy){
                    if(lx<0||lx>=L.w||fy<0||fy>=L.h) continue;
                    if(!g.stand(lx, fy)) continue;
                    RSet from_heart = harness::reachable_from(L, wm, lx, fy);
                    if(reaches_forward_exit(L, from_heart)) returnable = true;
                }
            }
            CHECK(returnable);
            std::printf("  [heart] room %d H(%d,%d): reachable=%s returnable=%s\n",
                        r, hc.tx, hc.ty, reach?"yes":"NO", returnable?"yes":"NO");
            ++checked;
        }
    }
    std::printf("  [heart] %d heart alcoves checked\n", checked);
    CHECK(checked >= 1);
}

// 7. No one-way traps: for every cracked-floor LANDING anchor, flooding from that landing must re-reach a
//    forward exit (the cage/exit counts, so the intended terminal passes too).
TEST(d7_no_one_way_traps){
    int checked = 0, terminals = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        harness::Grid g = harness::build_grid(L, base_open_wm(L));
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            int land = pound_landing_row(L, g, tx, ty);
            if(land < 0) continue;
            RSet from_land = harness::reachable_from(L, stone_cleared_wm(L), tx, land);
            bool can_leave = reaches_forward_exit(L, from_land);
            bool is_terminal = (L.has_cage && stands_at(L, from_land, L.cage_tx, L.cage_ty))
                            || (L.has_exit && stands_at(L, from_land, L.exit_tx, L.exit_ty));
            if(is_terminal){ ++terminals; }
            CHECK(can_leave);   // reaches_forward_exit includes cage/exit, so terminals also pass
            if(!can_leave)
                std::printf("  [oneway] room %d k(%d,%d) land row %d -> ONE-WAY TRAP (can't leave)\n",
                            r,tx,ty,land);
            ++checked;
        }
    }
    std::printf("  [oneway] %d cracked-floor landings checked (%d terminal)\n", checked, terminals);
    CHECK(checked >= 1);
}

// ===========================================================================
// ROOM-1 / ROOM-2 "REQUIRES-X" GATING INVARIANTS (QA round 2). BASE movement (proves the gate) vs the
// FULL path (Stone pound / Ice freeze). Must-NOT-bypass proofs flood at CLIMB_MAX (climb_max=true).
// ===========================================================================

// 10. ROOM 1: the heart MUST require the Stone pound. The alcove is sealed on all sides except a 2-wide
//     cracked-floor ceiling directly above it; BASE movement (cracked SOLID) cannot reach it even at the
//     edge apex jump (CLIMB_MAX), and the FULL kit (cracked smashed -> a 2-wide drop) can.
TEST(d7_room1_heart_requires_pound){
    const LevelData& L = DUNGEON7_ROOM1_DATA;
    CHECK(L.heart_container_count >= 1);
    int checked = 0;
    for(int i=0;i<L.heart_container_count;++i){
        const HeartContainerSpawn& hc = L.heart_containers[i];
        int sx = harness::room_start_x(L), sy = harness::room_start_y(L);
        // (a) FULL-TRAVERSAL-MINUS-POUND: boulders broken + loose dropped but cracked RE-SOLIDIFIED. The
        //     heart must NOT be reachable even at the edge apex jump (CLIMB_MAX) — only pounding the 2-wide
        //     cracked ceiling drops you in.
        harness::WorldModel base = no_pound_wm(L); base.climb_max = true;
        RSet base_seen = harness::reachable_from(L, base, sx, sy);
        bool base_reach = stands_at(L, base_seen, hc.tx, hc.ty);
        CHECK(!base_reach);          // GATED: base movement cannot reach the heart.

        // (b) a cracked floor sits above the heart in its 2-wide footprint (the pound-through ceiling).
        bool kk_above = false;
        for(int gi=0; gi<L.gate_count; ++gi){
            if(L.gates[gi].type!=GateType::CrackedFloor) continue;
            if(L.gates[gi].ty < hc.ty &&
               (L.gates[gi].tx==hc.tx || L.gates[gi].tx==hc.tx+1 || L.gates[gi].tx==hc.tx-1))
                kk_above = true;
        }
        CHECK(kk_above);             // a pound-through ceiling exists above the heart.

        // (c) FULL kit: cracked smashed -> the heart IS reachable from the entrance (RELIABLE reach).
        RSet full_seen = harness::reachable_from(L, stone_cleared_wm(L), sx, sy);
        bool full_reach = stands_at(L, full_seen, hc.tx, hc.ty);
        CHECK(full_reach);           // UNGATED with the Stone pound.

        std::printf("  [r1-heart] H(%d,%d): base=%s full=%s kk_above=%s\n",
                    hc.tx, hc.ty, base_reach?"REACHABLE(bad)":"gated",
                    full_reach?"reachable":"UNREACHABLE(bad)", kk_above?"yes":"NO");
        ++checked;
    }
    CHECK(checked >= 1);
}

// 11. ROOM 2: the spronk/exit MUST require the Ice freeze. With BASE movement (water a non-standable
//     hazard, cracked smashed) the cage C and exit E are NOT reachable from the entrance even at the edge
//     apex jump (CLIMB_MAX); once the water is frozen to a standable bridge they become RELIABLY reachable.
TEST(d7_room2_spronk_requires_freeze){
    const LevelData& L = DUNGEON7_ROOM2_DATA;
    CHECK(L.has_cage); CHECK(L.has_exit);
    int sx = harness::room_start_x(L), sy = harness::room_start_y(L);

    // (a) FULL-KIT-MINUS-FREEZE: cracked SMASHED but water left a non-standable HAZARD. C and E must be
    //     unreachable even at CLIMB_MAX — the water run is the only crossing (no jump-over bypass).
    harness::WorldModel base = stone_cleared_wm(L); base.climb_max = true;
    RSet base_seen = harness::reachable_from(L, base, sx, sy);
    bool base_c = stands_at(L, base_seen, L.cage_tx, L.cage_ty);
    bool base_e = stands_at(L, base_seen, L.exit_tx, L.exit_ty);
    CHECK(!base_c); CHECK(!base_e);     // GATED by the water (no jump-over bypass).

    // (b) FULL kit: freeze the water (standable bridge) + Stone-clear cracked floors. C and E reachable.
    harness::WorldModel full = stone_cleared_wm(L); full.water_frozen = true;
    RSet full_seen = harness::reachable_from(L, full, sx, sy);
    bool full_c = stands_at(L, full_seen, L.cage_tx, L.cage_ty);
    bool full_e = stands_at(L, full_seen, L.exit_tx, L.exit_ty);
    CHECK(full_c); CHECK(full_e);       // UNGATED once the water is frozen.

    std::printf("  [r2-freeze] C(%d,%d) E(%d,%d): base=(%s,%s) frozen=(%s,%s)\n",
                L.cage_tx,L.cage_ty,L.exit_tx,L.exit_ty,
                base_c?"reach(bad)":"gated", base_e?"reach(bad)":"gated",
                full_c?"reach":"MISS(bad)", full_e?"reach":"MISS(bad)");
}

// 12. ROOM 2: no exit soft-lock. From the cracked-floor DESCENT landing tile, BOTH C and E are reachable
//     (the player who pounds down can free the spronk AND leave).
TEST(d7_room2_exit_reachable_from_descent){
    const LevelData& L = DUNGEON7_ROOM2_DATA;
    CHECK(L.has_cage); CHECK(L.has_exit);
    harness::Grid g = harness::build_grid(L, base_open_wm(L));   // for the pound-landing scan (cracked solid)
    int checked = 0;
    for(int i=0;i<L.gate_count;++i){
        if(L.gates[i].type!=GateType::CrackedFloor) continue;
        int tx=L.gates[i].tx, ty=L.gates[i].ty;
        int land = pound_landing_row(L, g, tx, ty);
        CHECK(land >= 0); if(land<0) continue;
        RSet R = harness::reachable_from(L, stone_cleared_wm(L), tx, land);
        bool c_ok = stands_at(L, R, L.cage_tx, L.cage_ty);
        bool e_ok = stands_at(L, R, L.exit_tx, L.exit_ty);
        CHECK(c_ok); CHECK(e_ok);                 // both the spronk AND the exit reachable -> no soft-lock.
        std::printf("  [r2-descent] k(%d,%d) lands row %d -> C=%s E=%s\n",
                    tx,ty,land, c_ok?"reached":"STUCK", e_ok?"reached":"STUCK");
        ++checked;
    }
    CHECK(checked >= 1);
}

// ===========================================================================
// ROOM 0 — QA-driven gating + re-entry-safety invariants (each FAILS on a broken layout).
// ===========================================================================
static const RoomDoorSpawn* d7_find_door(const LevelData& L, int target_room){
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==target_room) return &L.room_doors[i];
    return nullptr;
}

// 7b. DOOR GROUNDING (anti-floating-archway). Every Room-0 door column grounds cleanly on the MAIN bottom
//     floor (row h-2), with all DarkVeil gates CLOSED (their render-faithful 2-wide fill). A door sitting
//     in a gate-fill column would ground on the fill (row h-3) instead -> RED.
TEST(d7_room0_doors_ground_on_main_floor){
    const LevelData& L = DUNGEON7_ROOM0_DATA;
    const int floor_row = L.h - 2;                 // bottom interior floor row (row 20 for h=22)
    // Render-faithful solid map with every DarkVeil gate CLOSED — reuse the harness grid (default
    // WorldModel fills every non-open gate as a 2-wide full-height column, mirroring scene_dungeon.cpp).
    harness::Grid g = harness::build_grid(L, harness::WorldModel{});
    int checked=0;
    for(int i=0;i<L.room_door_count;++i){
        const RoomDoorSpawn& d = L.room_doors[i];
        for(int dx=0; dx<2; ++dx){                  // the archway is 2-wide (cols d.tx, d.tx+1)
            int col=d.tx+dx;
            int fr=-1;
            for(int y=d.ty+1; y<L.h; ++y) if(g.solid[y*L.w+col]){ fr=y; break; }
            CHECK(fr==floor_row);                   // grounds on the MAIN floor, not a gate fill/wall
            if(fr!=floor_row)
                std::printf("  [ground] door(%d,%d) col %d grounds at row %d (want %d) -> FLOATING\n",
                            d.tx,d.ty,col,fr,floor_row);
            ++checked;
        }
    }
    std::printf("  [ground] %d door columns checked (all grounded on row %d)\n", checked, floor_row);
    CHECK(checked>=4);   // >=2 doors x 2 columns
}

// 8. The Room-1 door MUST be gated behind the heavy switch: with the DarkVeil gate CLOSED and no pound,
//    the Room-1 door is NOT reachable from spawn (even at CLIMB_MAX); once the heavy switch fires its
//    open_column (opening the gate), it BECOMES reachable (RELIABLE).
TEST(d7_room0_room1_requires_heavy_switch){
    const LevelData& L = DUNGEON7_ROOM0_DATA;
    const RoomDoorSpawn* r1 = d7_find_door(L, 1);
    REQUIRE(r1 != nullptr);
    // BASE KIT: cracked solid + DarkVeil CLOSED (harness default fills the gate). Edge apex jump can't bypass.
    harness::WorldModel closed{}; closed.climb_max = true;
    RSet seen_closed = harness::reachable_from(L, closed, L.spawn_tx, L.spawn_ty);
    bool reachable_closed = stands_at(L, seen_closed, r1->tx, r1->ty);
    CHECK(!reachable_closed);     // GATED: cannot reach the Room-1 door without opening the gate.

    // OPEN the heavy switch's gate column(s) via open_columns (mirror scene_dungeon.cpp open_column).
    int heavy_target = -1;
    for(int i=0;i<L.plate_count;++i) if(L.plates[i].heavy) heavy_target = L.plates[i].target_tx;
    CHECK(heavy_target >= 0);
    harness::WorldModel opened{}; opened.open_columns.insert(heavy_target);
    RSet seen_open = harness::reachable_from(L, opened, L.spawn_tx, L.spawn_ty);
    bool reachable_open = stands_at(L, seen_open, r1->tx, r1->ty);
    CHECK(reachable_open);        // UNGATED once the heavy switch opens the DarkVeil gate.
    std::printf("  [r1-gate] Room-1 door (%d,%d): closed=%s open=%s\n",
                r1->tx, r1->ty, reachable_closed?"REACHABLE(bad)":"gated", reachable_open?"reachable":"STILL-GATED(bad)");
}

// 9. RE-ENTRY SOFT-LOCK GUARD: with ALL cracked floors respawned SOLID and ALL gates CLOSED (the re-entry
//    load state), the hub-return 'Q' door AND the Room-2 door must be reachable from the dungeon-entry
//    spawn AND from EACH return entrance — WITHOUT pounding.
TEST(d7_room0_hub_exit_and_room2_always_reachable){
    const LevelData& L = DUNGEON7_ROOM0_DATA;
    const RoomDoorSpawn* q  = d7_find_door(L, -1);   // hub-exit 'Q'
    const RoomDoorSpawn* r2 = d7_find_door(L, 2);    // Room-2 door
    REQUIRE(q  != nullptr);
    REQUIRE(r2 != nullptr);

    // Re-entry grid: cracked floors SOLID + DarkVeil CLOSED = the harness default WorldModel. No pound, no
    // open. Flooded at RELIABLE (the player must reliably re-reach both doors).
    harness::WorldModel wm{};

    struct Seed { const char* name; int x, y; };
    std::vector<Seed> seeds;
    seeds.push_back({"spawn", L.spawn_tx, L.spawn_ty});
    for(int e=0;e<L.entrance_count;++e)
        seeds.push_back({"entrance", L.entrances[e].tx, L.entrances[e].ty});

    int verified = 0;
    for(const Seed& s : seeds){
        RSet seen = harness::reachable_from(L, wm, s.x, s.y);
        bool q_ok  = stands_at(L, seen, q->tx,  q->ty);
        bool r2_ok = stands_at(L, seen, r2->tx, r2->ty);
        CHECK(q_ok);
        CHECK(r2_ok);
        std::printf("  [reentry] from %s (%d,%d): Q=%s Room2=%s\n",
                    s.name, s.x, s.y, q_ok?"reachable":"STRANDED", r2_ok?"reachable":"STRANDED");
        ++verified;
    }
    CHECK(verified >= 3);   // spawn + id1 + id2
}
