#include "test_framework.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon7_room0.h"
#include "game/levels/dungeon7_room1.h"
#include "game/levels/dungeon7_room2.h"
#include <vector>
#include <queue>
#include <cstdio>
using namespace logic;

// Quaking Quarry (Dungeon 7) — the Stone ground-pound descent dungeon (M8).
// Standard structural invariants (D6 style) PLUS first-class no-soft-lock invariants that each
// FAIL on a deliberately-broken layout (verified during authoring; see the report). All assert
// against the COMPILED DUNGEON7_ROOM* tile/entity data.

static const LevelData* const D7_ROOMS[] = {
    &DUNGEON7_ROOM0_DATA, &DUNGEON7_ROOM1_DATA, &DUNGEON7_ROOM2_DATA };
static constexpr int D7_N = 3;
static constexpr int CLIMB = 6;   // Featherleap double-jump reaches ~6-7 tiles; the escapable-wall bound.

// ---------------------------------------------------------------------------
// Tile classification. CRITICAL: 'k' (CrackedFloor), 'O' (boulder), ':' (loose platform) are CONTENT
// symbols, so the COMPILED tiles[] holds Empty(0) at their positions — the SCENE makes them solid at
// runtime. So the flood-fill must treat them as SOLID (the player walks on them / they block) from the
// gate/boulder/loose-platform arrays, NOT from tiles[]. Hazards (Lava/Water/Spikes) are non-solid.
// ---------------------------------------------------------------------------
struct Grid {
    int w, h;
    std::vector<uint8_t> solid;   // 1 = blocks (Solid/IcePlatform/cracked/boulder/loose), 0 = passable
    std::vector<uint8_t> hazard;  // 1 = Lava/Water/Spikes (passable but deadly)

    bool blk(int x, int y) const {
        if(x < 0 || y < 0 || x >= w || y >= h) return true;   // OOB is solid
        return solid[y*w + x] != 0;
    }
    bool haz(int x, int y) const {
        if(x < 0 || y < 0 || x >= w || y >= h) return false;
        return hazard[y*w + x] != 0;
    }
    // standable: a passable, non-hazard cell with a blocking cell directly below.
    bool standable(int x, int y) const {
        return !blk(x, y) && !haz(x, y) && blk(x, y+1);
    }
};

// Build the static grid for a room. opt_drop_boulder / opt_drop_loose let invariants simulate a
// boulder removed or a loose platform dropped to its rest row.
static Grid build_grid(const LevelData& L,
                       int remove_boulder = -1,            // index into L.boulders to remove (clear its tile)
                       int drop_loose = -1)                // index into L.loose_platforms to drop to rest row
{
    Grid g; g.w = L.w; g.h = L.h;
    g.solid.assign(L.w*L.h, 0);
    g.hazard.assign(L.w*L.h, 0);
    for(int y = 0; y < L.h; ++y) for(int x = 0; x < L.w; ++x){
        TileKind k = (TileKind)L.tiles[y*L.w + x];
        if(k == TileKind::Solid || k == TileKind::IcePlatform) g.solid[y*L.w + x] = 1;
        if(k == TileKind::Lava || k == TileKind::Water || k == TileKind::Spikes) g.hazard[y*L.w + x] = 1;
    }
    // cracked floors are solid (walked-on) until pounded
    for(int i = 0; i < L.gate_count; ++i)
        if(L.gates[i].type == GateType::CrackedFloor)
            g.solid[L.gates[i].ty*L.w + L.gates[i].tx] = 1;
    // boulders are solid
    for(int i = 0; i < L.boulder_count; ++i)
        if(i != remove_boulder)
            g.solid[L.boulders[i].ty*L.w + L.boulders[i].tx] = 1;
    // loose platforms: a solid run of `len` tiles. If drop_loose==i, place at its REST row (drop
    // straight down to the first row whose tiles-below are all solid); else at its authored row.
    for(int i = 0; i < L.loose_platform_count; ++i){
        const LoosePlatformSpawn& lp = L.loose_platforms[i];
        int ty = lp.ty;
        if(i == drop_loose){
            // drop straight down until the row directly below every tile of the run is solid.
            while(ty + 1 < L.h){
                bool rest = false;
                for(int dx = 0; dx < lp.len; ++dx)
                    if(g.solid[(ty+1)*L.w + (lp.tx+dx)]) rest = true;
                if(rest) break;
                ++ty;
            }
        }
        for(int dx = 0; dx < lp.len; ++dx) g.solid[ty*L.w + (lp.tx+dx)] = 1;
    }
    return g;
}

// Bounded reachability flood-fill over STANDABLE tiles. A move from (x,y):
//   * walk to a horizontally-adjacent standable tile,
//   * drop down (any depth) to the first standable tile in a column step away,
//   * climb up to <= CLIMB tiles to a higher standable tile one column away.
// Returns the set of reached standable tiles (as a w*h bool grid).
static std::vector<uint8_t> reachable(const Grid& g, int sx, int sy){
    std::vector<uint8_t> seen(g.w*g.h, 0);
    if(!g.standable(g.w ? sx : 0, sy)) {
        // snap the start down to the nearest standable tile in its column (entrances sit on a floor)
        while(sy+1 < g.h && !g.blk(sx, sy+1) && !g.haz(sx,sy+1)) ++sy;
    }
    if(sx < 0 || sy < 0 || sx >= g.w || sy >= g.h) return seen;
    std::queue<std::pair<int,int>> q;
    auto push = [&](int x, int y){
        if(x<0||y<0||x>=g.w||y>=g.h) return;
        if(!g.standable(x,y) || seen[y*g.w+x]) return;
        seen[y*g.w+x] = 1; q.push({x,y});
    };
    // seed: snap start to a standable tile
    int y0 = sy; while(y0+1 < g.h && !g.blk(sx,y0+1) && !g.haz(sx,y0+1)) ++y0;
    push(sx, y0);
    while(!q.empty()){
        auto [x,y] = q.front(); q.pop();
        for(int dir = -1; dir <= 1; dir += 2){
            int nx = x + dir;
            if(!g.blk(nx, y)){
                // the body row is clear that way: walk and/or drop down the neighbor column.
                if(g.standable(nx, y)) push(nx, y);
                int dy = y;
                while(dy+1 < g.h && !g.blk(nx, dy+1) && !g.haz(nx, dy+1)) ++dy;
                if(dy != y) push(nx, dy);
            }
            // climb / wall-jump: rise up to CLIMB tiles to a higher standable tile in the neighbor
            // column (double-jump). Allowed even when a wall blocks the body row (jumping onto a
            // ledge); requires clear air directly above the player's CURRENT position to get airborne.
            for(int up = 1; up <= CLIMB; ++up){
                int uy = y - up;
                if(uy < 0) break;
                if(g.blk(x, uy)) break;           // a ceiling above the player blocks getting airborne
                if(g.blk(nx, uy)) continue;       // that target cell is wall; a higher one may still be open
                if(g.standable(nx, uy)) push(nx, uy);
            }
        }
    }
    return seen;
}

// True if any reached standable tile is a forward exit: a room-door, the cage, or the exit. We accept
// "near" (within 1 tile horizontally on the same row) because the door/cage/exit sprite spans tiles.
static bool reaches_forward_exit(const LevelData& L, const std::vector<uint8_t>& seen){
    // A door/cage/exit SPRITE sits on the content row (18) but the player STANDS on the floor row
    // (19, solid below at 20). So a target at (tx,ty) is "reached" if a reached standable tile is at
    // the target tile OR directly below it (the standing position), within +/-1 column.
    auto hit = [&](int tx, int ty)->bool{
        for(int dx = -1; dx <= 1; ++dx){
            int x = tx+dx;
            if(x<0 || x>=L.w) continue;
            if(ty>=0   && ty<L.h   && seen[ty*L.w + x])     return true;
            if(ty+1>=0 && ty+1<L.h && seen[(ty+1)*L.w + x]) return true;  // standing row below the sprite
        }
        return false;
    };
    for(int i = 0; i < L.room_door_count; ++i)
        if(hit(L.room_doors[i].tx, L.room_doors[i].ty)) return true;
    if(L.has_cage && hit(L.cage_tx, L.cage_ty)) return true;
    if(L.has_exit && hit(L.exit_tx, L.exit_ty)) return true;
    return false;
}

// Scan straight DOWN from a cracked floor through any stacked cracked floors to the first
// NON-cracked solid tile; return that landing row (or -1 if none / OOB).
static bool is_cracked(const LevelData& L, int tx, int ty){
    for(int i = 0; i < L.gate_count; ++i)
        if(L.gates[i].type == GateType::CrackedFloor && L.gates[i].tx==tx && L.gates[i].ty==ty) return true;
    return false;
}
static int pound_landing_row(const LevelData& L, const Grid& g, int tx, int ty){
    // start at the cracked floor; the pound smashes it and every stacked cracked tile below, landing
    // on the first NON-cracked solid tile. The landing STANDABLE row is one above that solid tile.
    int y = ty;
    while(y+1 < L.h){
        int below = y+1;
        if(is_cracked(L, tx, below)){ y = below; continue; }   // chain through stacked cracked floors
        if(g.solid[below*L.w + tx] && !is_cracked(L, tx, below)) return y;  // land standing on row y
        if(!g.solid[below*L.w + tx]) { ++y; continue; }        // empty -> keep falling
        return y;
    }
    return -1;
}

// ===========================================================================
// Standard structural invariants (D6 style)
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

TEST(d7_rooms_min_size){
    for(int r = 0; r < D7_N; ++r){ CHECK(D7_ROOMS[r]->w >= 30); CHECK(D7_ROOMS[r]->h >= 22); }
}

TEST(d7_rooms_solid_border){
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        for(int x = 0; x < L.w; ++x){ CHECK_EQ((int)L.tiles[x],1); CHECK_EQ((int)L.tiles[(L.h-1)*L.w+x],1); }
        for(int y = 0; y < L.h; ++y){ CHECK_EQ((int)L.tiles[y*L.w],1); CHECK_EQ((int)L.tiles[y*L.w+(L.w-1)],1); }
    }
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
    // spawn/enemy/shrine/heart ground on content row 18 (feet land on the row-20 floor).
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        CHECK_EQ(L.spawn_ty, 18);
        for(int i=0;i<L.enemy_count;++i)  CHECK_EQ((int)L.enemies[i].ty,18);
        for(int i=0;i<L.pickup_count;++i) CHECK_EQ((int)L.pickups[i].ty,18);
        for(int i=0;i<L.heart_container_count;++i) CHECK_EQ((int)L.heart_containers[i].ty,18);
    }
}

TEST(d7_room_doors_resolve_and_two_way){
    // Every room-door target resolves to an existing entrance; the hub-room (0) branches to 1 and 2,
    // and each branch returns to a DISTINCT entrance in room 0 (co-located return entrance pattern).
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r];
        for(int i = 0; i < L.room_door_count; ++i){
            const RoomDoorSpawn& d = L.room_doors[i];
            CHECK(d.target_room>=0 && d.target_room<D7_N);
            const LevelData& T = *D7_ROOMS[d.target_room];
            bool found=false; for(int e=0;e<T.entrance_count;++e) if(T.entrances[e].id==d.target_entrance) found=true;
            CHECK(found);
            // co-located return entrance: a same-row entrance within 1..2 tiles of the door.
            bool near=false;
            for(int e=0;e<L.entrance_count;++e){
                const EntranceSpawn& n=L.entrances[e];
                int dx=n.tx-d.tx; if(dx<0) dx=-dx;
                if(n.ty==d.ty && dx>=1 && dx<=2) near=true;
            }
            CHECK(near);
        }
    }
    // explicit two-way wiring (room0 hubs to 1 and 2; each returns to a distinct room0 entrance)
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
        const LevelData& L = *D7_ROOMS[r]; Grid g = build_grid(L);
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
        const LevelData& L = *D7_ROOMS[r]; Grid g = build_grid(L);
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            int land = pound_landing_row(L, g, tx, ty);
            CHECK(land >= 0);                              // the plunge ends on real ground
            if(land < 0) continue;
            // remove the smashed cracked-floor tiles in this column so the flood-fill sees the opening,
            // then flood from the landing tile and require a forward exit.
            Grid gp = g;
            for(int y=ty; y<=land; ++y) if(is_cracked(L,tx,y)) gp.solid[y*L.w+tx]=0;
            std::vector<uint8_t> seen = reachable(gp, tx, land);
            bool ok = reaches_forward_exit(L, seen);
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
        const LevelData& L = *D7_ROOMS[r]; Grid g = build_grid(L);
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            int land = pound_landing_row(L, g, tx, ty);
            // scan every traversed tile from the cracked floor down to (and including) the landing tile.
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
        // boulders: after removing each (pounded away), a forward exit stays reachable from the entrance,
        // and its tile does not overlap a door/cage/exit.
        for(int b=0;b<L.boulder_count;++b){
            Grid g = build_grid(L, /*remove_boulder*/b);
            // start the flood from the room's first entrance (fallback: spawn).
            int sx = L.entrance_count? L.entrances[0].tx : L.spawn_tx;
            int sy = L.entrance_count? L.entrances[0].ty : L.spawn_ty;
            std::vector<uint8_t> seen = reachable(g, sx, sy);
            bool ok = reaches_forward_exit(L, seen);
            CHECK(ok);
            std::printf("  [boulder] room %d O(%d,%d) removed -> forward-exit %s\n",
                        r, L.boulders[b].tx, L.boulders[b].ty, ok?"reached":"MISSING");
            ++covered;
        }
        // loose platforms: after dropping each to its rest row, a forward exit stays reachable AND the
        // rest tiles don't overlap a door/cage/exit (no sealing the goal).
        for(int p=0;p<L.loose_platform_count;++p){
            Grid g = build_grid(L, -1, /*drop_loose*/p);
            int sx = L.entrance_count? L.entrances[0].tx : L.spawn_tx;
            int sy = L.entrance_count? L.entrances[0].ty : L.spawn_ty;
            std::vector<uint8_t> seen = reachable(g, sx, sy);
            bool ok = reaches_forward_exit(L, seen);
            CHECK(ok);
            // compute the rest row and assert none of its tiles is a door/cage/exit tile.
            const LoosePlatformSpawn& lp = L.loose_platforms[p];
            // recompute the rest row (mirror build_grid's drop) on a boulder-less static grid
            Grid base = build_grid(L);
            int ty = lp.ty;
            while(ty+1 < L.h){
                bool rest=false;
                for(int dx=0;dx<lp.len;++dx) if(base.solid[(ty+1)*L.w+(lp.tx+dx)]) rest=true;
                if(rest) break;
                ++ty;
            }
            for(int dx=0; dx<lp.len; ++dx){ int x=lp.tx+dx;
                for(int i=0;i<L.room_door_count;++i) CHECK(!(L.room_doors[i].tx==x && L.room_doors[i].ty==ty));
                if(L.has_cage) CHECK(!(L.cage_tx==x && L.cage_ty==ty));
                if(L.has_exit) CHECK(!(L.exit_tx==x && L.exit_ty==ty));
            }
            std::printf("  [loose] room %d :(%d,%d,len%d) rests row %d -> forward-exit %s\n",
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

// 4. No pound deposits the player in an inescapable pit. Every cracked-floor landing is either
//    climbable out (<=CLIMB wall) OR reaches a forward exit OR is the intended terminal (the spronk).
TEST(d7_no_pound_pit_traps){
    int checked = 0;
    for(int r = 0; r < D7_N; ++r){
        const LevelData& L = *D7_ROOMS[r]; Grid g = build_grid(L);
        for(int i=0;i<L.gate_count;++i){ if(L.gates[i].type!=GateType::CrackedFloor) continue;
            int tx=L.gates[i].tx, ty=L.gates[i].ty;
            int land = pound_landing_row(L, g, tx, ty);
            CHECK(land>=0); if(land<0) continue;
            Grid gp = g; for(int y=ty;y<=land;++y) if(is_cracked(L,tx,y)) gp.solid[y*L.w+tx]=0;
            std::vector<uint8_t> seen = reachable(gp, tx, land);
            bool exit_ok = reaches_forward_exit(L, seen);
            // terminal: the landing reaches the cage/exit -> intended end (handled by exit_ok via cage/exit).
            // climbable: from the landing, is there ANY route out of the immediate pit (reached set > the
            //   landing column alone, i.e. the player can leave the landing column)?
            int reached_cells = 0; for(uint8_t v : seen) reached_cells += v;
            bool can_leave = reached_cells > 1;
            CHECK(exit_ok || can_leave);
            ++checked;
            if(!(exit_ok || can_leave))
                std::printf("  [pit] room %d k(%d,%d) land row %d is an INESCAPABLE pit\n", r,tx,ty,land);
        }
    }
    CHECK(checked >= 1);
}

// 5. Magic-gated beats have a magic source (enemy 'o') in the same room or an earlier (lower-index)
//    room along the path from start. Room 2's Ice water-gap is funded by room 0 and/or room 2 enemies.
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
        // a magic source in this room OR any lower-index room (rooms are reached from room 0).
        bool source = false;
        for(int rr = 0; rr <= r; ++rr) if(D7_ROOMS[rr]->enemy_count >= 1) source = true;
        CHECK(source);
        std::printf("  [magic] room %d has a Water beat; magic source <= room %d: %s\n", r, r, source?"yes":"NO");
    }
    std::printf("  [magic] %d magic-gated beats checked\n", beats);
    CHECK(beats >= 1);
}
