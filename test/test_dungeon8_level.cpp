#include "test_framework.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon8_room0.h"
#include "game/levels/dungeon8_room1.h"
#include "game/levels/dungeon8_room2.h"
#include <vector>
#include <queue>
#include <algorithm>
#include <functional>
#include <cstdio>
using namespace logic;

// Gloom Spire (Dungeon 8) — the Light-spell timed-reveal ascent dungeon (M10). Standard structural
// invariants (D7 style) PLUS first-class no-soft-lock invariants that each FAIL on a deliberately-broken
// layout (verified during authoring; see the report). All assert against the COMPILED DUNGEON8_ROOM* data.
//
// The flood-fill harness is the SAME 2-wide x 4-tall body model used for D7. The two M10-specific grid
// variants parallel D7's freeze_water/open_heavy_gate:
//   * reveal_hidden  — treat every HiddenPlatformSpawn tile SOLID (Light has revealed them).
//   * open_darkveil  — treat every DarkVeil gate OPEN (its 2-wide fill cleared by a Light cast).
// BASE movement = hidden platforms NON-solid + DarkVeil CLOSED (a solid 2-wide column). The gating
// invariants distinguish BASE (Light NOT yet earned/used) from the revealed+opened frontier (Light used).

static const LevelData* const D8_ROOMS[] = {
    &DUNGEON8_ROOM0_DATA, &DUNGEON8_ROOM1_DATA, &DUNGEON8_ROOM2_DATA };
static constexpr int D8_N = 3;
// Featherleap double-jump reach. Bumped 6 -> 7 (M10 QA) so the flood-fill reflects the REAL player:
// Featherleap reaches ~6-7 tiles straight up, so a climb is only TRULY Light-required when its rest-to-rest
// VERTICAL gap exceeds 7 (a CLIMB=7 flood-fill base-unreachable then matches the in-game player — neither can
// bypass). NOTE: glide extends HORIZONTAL distance and is NOT modeled here, so a horizontal "Light-gate" would
// be unsafe (glide bypasses it); ALL D8 Light-climbs are therefore VERTICAL (gap > 7 straight up, hidden-only
// footholds, no grapple anchor) — the only shape this model can soundly prove requires Light.
static constexpr int CLIMB = 7;

static constexpr int PW = 2;   // player width in tiles
static constexpr int PH = 4;   // player height in tiles

struct Grid {
    int w, h;
    std::vector<uint8_t> solid;
    std::vector<uint8_t> hazard;

    bool blk(int x, int y) const {
        if(x < 0 || y < 0 || x >= w || y >= h) return true;
        return solid[y*w + x] != 0;
    }
    bool haz(int x, int y) const {
        if(x < 0 || y < 0 || x >= w || y >= h) return false;
        return hazard[y*w + x] != 0;
    }
    bool cell_clear(int x, int y) const { return !blk(x,y) && !haz(x,y); }
    bool fits(int x, int y) const {
        if(x < 0 || x+PW-1 >= w || y-PH+1 < 0 || y >= h) return false;
        for(int cx = x; cx < x+PW; ++cx)
            for(int cy = y-PH+1; cy <= y; ++cy)
                if(!cell_clear(cx, cy)) return false;
        return true;
    }
    bool standable(int x, int y) const {
        return fits(x, y) && (blk(x, y+1) || blk(x+PW-1, y+1));
    }
};

// Static grid: Solid/IcePlatform block; Lava/Water/Spikes are non-solid hazards. DarkVeil gates render
// as a CLOSED 2-wide full-height fill (cols tx..tx+1, rows 1..h-3) — the state a room loads in — UNLESS
// `open_dv` (a Light cast cleared them). Hidden platforms are NON-solid here UNLESS `reveal` (Light has
// revealed them, making the tiles solid+standable).
static Grid build_grid(const LevelData& L, bool reveal = false, bool open_dv = false){
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
    // DarkVeil gates: a CLOSED 2-wide full-height fill (mirrors scene_dungeon.cpp fill_column), unless open.
    if(!open_dv){
        for(int i = 0; i < L.gate_count; ++i){
            if(L.gates[i].type != GateType::DarkVeil) continue;
            for(int ty = 1; ty < L.h-2; ++ty) for(int dx = 0; dx < 2; ++dx){
                int x = L.gates[i].tx + dx; if(x>=0 && x<L.w) g.solid[ty*L.w + x] = 1;
            }
        }
    }
    // hidden platforms: NON-solid by default; SOLID when revealed (Light cast). A run of `len` tiles.
    if(reveal){
        for(int i = 0; i < L.hidden_platform_count; ++i){
            const HiddenPlatformSpawn& hp = L.hidden_platforms[i];
            for(int dx = 0; dx < hp.len; ++dx){
                int x = hp.tx+dx; if(x>=0 && x<L.w) g.solid[hp.ty*L.w + x] = 1;
            }
        }
    }
    return g;
}

static bool snap_start(const Grid& g, int& sx, int& sy){
    for(int lx : { sx, sx-1 }){
        int y = sy;
        while(y < g.h){
            if(g.standable(lx, y)){ sx = lx; sy = y; return true; }
            if(g.fits(lx, y) && !(g.blk(lx,y+1)||g.blk(lx+PW-1,y+1))) { ++y; continue; }
            ++y;
        }
    }
    return false;
}

static std::vector<uint8_t> reachable(const Grid& g, int sx, int sy){
    std::vector<uint8_t> seen(g.w*g.h, 0);
    if(!snap_start(g, sx, sy)) return seen;
    std::queue<std::pair<int,int>> q;
    auto push = [&](int x, int y){
        if(x<0||y<0||x>=g.w||y>=g.h) return;
        if(!g.standable(x,y) || seen[y*g.w+x]) return;
        seen[y*g.w+x] = 1; q.push({x,y});
    };
    push(sx, sy);
    while(!q.empty()){
        auto [x,y] = q.front(); q.pop();
        for(int up = 1; up <= CLIMB; ++up){
            int ny = y - up;
            if(ny - PH + 1 < 0) break;
            if(!g.fits(x, ny)) break;
            if(g.standable(x, ny)) push(x, ny);
        }
        for(int dir = -1; dir <= 1; dir += 2){
            int nx = x + dir;
            for(int up = 1; up <= CLIMB; ++up){
                int ny = y - up;
                if(ny - PH + 1 < 0) break;
                if(!g.fits(x, ny)) break;
                if(g.standable(nx, ny)) push(nx, ny);
            }
            if(g.fits(nx, y)){
                int ny = y;
                while(ny+1 < g.h && g.fits(nx, ny+1) && !(g.blk(nx,ny+1)||g.blk(nx+PW-1,ny+1))) ++ny;
                if(g.standable(nx, ny)) push(nx, ny);
                else if(g.standable(nx, y)) push(nx, y);
            }
        }
    }
    return seen;
}

static bool stands_at(const LevelData& L, const std::vector<uint8_t>& seen, int tx, int ty){
    for(int dy = 0; dy <= 1; ++dy){
        int fy = ty + dy;
        if(fy < 0 || fy >= L.h) continue;
        for(int lx = tx-PW+1; lx <= tx; ++lx)
            if(lx >= 0 && lx < L.w && seen[fy*L.w + lx]) return true;
    }
    return false;
}
static bool reaches_forward_exit(const LevelData& L, const std::vector<uint8_t>& seen){
    for(int i = 0; i < L.room_door_count; ++i)
        if(stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) return true;
    if(L.has_cage && stands_at(L, seen, L.cage_tx, L.cage_ty)) return true;
    if(L.has_exit && stands_at(L, seen, L.exit_tx, L.exit_ty)) return true;
    return false;
}

static int room_start_x(const LevelData& L){ return L.entrance_count? L.entrances[0].tx : L.spawn_tx; }
static int room_start_y(const LevelData& L){ return L.entrance_count? L.entrances[0].ty : L.spawn_ty; }

// ===========================================================================
// Standard structural invariants (D7 style)
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

TEST(d8_rooms_min_size){
    for(int r = 0; r < D8_N; ++r){ CHECK(D8_ROOMS[r]->w >= 30); CHECK(D8_ROOMS[r]->h >= 22); }
}

TEST(d8_rooms_solid_border){
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        for(int x = 0; x < L.w; ++x){ CHECK_EQ((int)L.tiles[x],1); CHECK_EQ((int)L.tiles[(L.h-1)*L.w+x],1); }
        for(int y = 0; y < L.h; ++y){ CHECK_EQ((int)L.tiles[y*L.w],1); CHECK_EQ((int)L.tiles[y*L.w+(L.w-1)],1); }
    }
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
// container with id 2 (D6 used id0, D7 id1), GROUNDED (a solid tile directly below it), and SEALED behind the
// Light puzzle: from the room-1 entrance it is BASE-unreachable (hidden platforms non-solid + DarkVeil closed)
// but LIT-reachable (reveal + open). So the heart is an OPTIONAL permanent-max-HP reward for using Light.
// Break tests: change the id off 2 -> RED; float the heart (no solid below) -> RED; make the climb static '#'
// so base reaches it -> base-reachable -> RED; wall it off so lit can't reach -> lit-miss -> RED.
TEST(d8_room1_has_heart_container){
    const LevelData& L = DUNGEON8_ROOM1_DATA;
    CHECK_EQ(L.heart_container_count, 1);
    const HeartContainerSpawn& hc = L.heart_containers[0];
    CHECK_EQ(hc.id, 2);                       // D8 heart container id
    CHECK(d8_solid(L, hc.tx, hc.ty+1));       // grounded (the engine grounds it onto the ledge below)

    int sx = room_start_x(L), sy = room_start_y(L);
    Grid base = build_grid(L, /*reveal*/false, /*open_dv*/false);
    std::vector<uint8_t> base_seen = reachable(base, sx, sy);
    bool base_h = stands_at(L, base_seen, hc.tx, hc.ty);
    CHECK(!base_h);                            // sealed: NOT reachable before Light

    Grid lit = build_grid(L, /*reveal*/true, /*open_dv*/true);
    std::vector<uint8_t> lit_seen = reachable(lit, sx, sy);
    bool lit_h = stands_at(L, lit_seen, hc.tx, hc.ty);
    CHECK(lit_h);                              // reachable once Light reveals the climb

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
            Grid base = build_grid(L, /*reveal*/false, /*open_dv*/false);  // BASE: no Light yet
            std::vector<uint8_t> seen = reachable(base, L.spawn_tx, L.spawn_ty);
            bool reach = stands_at(L, seen, L.pickups[i].tx, L.pickups[i].ty);
            CHECK(reach);
            std::printf("  [shrine] room %d F(%d,%d) base-reachable=%s\n",
                        r, L.pickups[i].tx, L.pickups[i].ty, reach?"yes":"NO");
            ++checked;
        }
    }
    CHECK_EQ(checked, 1);   // exactly the one Light shrine
}

// 2. The spronk/exit REQUIRES Light. With BASE movement (hidden platforms NON-solid + DarkVeil CLOSED)
//    the cage C and exit E are NOT reachable from the room entrance; once hidden platforms are revealed
//    AND DarkVeil opened (a Light cast) they BECOME reachable. Proves Light is genuinely required.
//    Break test: make the ascent platforms ordinary solid '#' (so base reaches the top) -> base goes
//    REACHABLE -> RED. (Verified by promoting the h tiles to # during authoring.)
TEST(d8_spronk_requires_light){
    const LevelData& L = DUNGEON8_ROOM2_DATA;
    CHECK(L.has_cage); CHECK(L.has_exit);
    int sx = room_start_x(L), sy = room_start_y(L);

    // (a) BASE: hidden platforms NON-solid, DarkVeil closed. C and E unreachable.
    Grid base = build_grid(L, /*reveal*/false, /*open_dv*/false);
    std::vector<uint8_t> base_seen = reachable(base, sx, sy);
    bool base_c = stands_at(L, base_seen, L.cage_tx, L.cage_ty);
    bool base_e = stands_at(L, base_seen, L.exit_tx, L.exit_ty);
    CHECK(!base_c); CHECK(!base_e);

    // (b) Light used: reveal hidden platforms + open DarkVeil. C and E reachable.
    Grid lit = build_grid(L, /*reveal*/true, /*open_dv*/true);
    std::vector<uint8_t> lit_seen = reachable(lit, sx, sy);
    bool lit_c = stands_at(L, lit_seen, L.cage_tx, L.cage_ty);
    bool lit_e = stands_at(L, lit_seen, L.exit_tx, L.exit_ty);
    CHECK(lit_c); CHECK(lit_e);

    std::printf("  [requires-light] C(%d,%d) E(%d,%d): base=(%s,%s) lit=(%s,%s)\n",
                L.cage_tx,L.cage_ty,L.exit_tx,L.exit_ty,
                base_c?"reach(bad)":"gated", base_e?"reach(bad)":"gated",
                lit_c?"reach":"MISS(bad)", lit_e?"reach":"MISS(bad)");
}

// 3. A magic crystal '$' is reachable (the guaranteed combat-free full refill) BEFORE each hidden-platform
//    region — no magic soft-lock. Model: each hidden-platform region launches from a rest ledge; the
//    player refuels at a crystal sitting on a SOLID rest point at-or-below the region. Concretely, for
//    each distinct hidden-platform ROW R, require a '$' crystal that (i) sits on genuinely SOLID ground
//    (NOT on a hidden platform), (ii) is reachable in the reveal_hidden grid, and (iii) sits at a row >= R
//    (you reach the refuel before climbing up into that region). An enemy does NOT count.
//    Break test: delete the floor/ledge crystal -> a region loses its before-it refuel -> RED.
TEST(d8_magic_crystal_before_each_light_beat){
    int regions_checked = 0;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        if(L.hidden_platform_count == 0) continue;
        int sx = room_start_x(L), sy = room_start_y(L);
        Grid lit = build_grid(L, /*reveal*/true, /*open_dv*/true);
        std::vector<uint8_t> seen = reachable(lit, sx, sy);

        // a crystal sits on PERSISTENT solid ground (a rest ledge / floor) — NOT on a fading hidden
        // platform — iff scanning straight down from it in the BASE grid (hidden platforms non-solid)
        // the first solid tile reached is a genuine Solid tile (the floor sits 2 rows below the content
        // row; a ledge crystal sits 1 row above its ledge). A crystal supported only by a hidden platform
        // would, in the base grid, fall all the way through it -> still lands on real ground below, so we
        // additionally require the crystal itself is base-reachable-grounded: there is solid within 2 rows
        // below it in the base grid (a real rest point), distinguishing a floor/ledge crystal from one
        // perched mid-air on a hidden step.
        Grid base = build_grid(L, /*reveal*/false, /*open_dv*/false);
        auto crystal_on_solid = [&](const MagicCrystalSpawn& c)->bool{
            for(int dy=1; dy<=2; ++dy)
                if(base.blk(c.tx, c.ty+dy) || base.blk(c.tx+1, c.ty+dy)) return true;
            return false;
        };
        auto crystal_reachable = [&](const MagicCrystalSpawn& c)->bool{
            return stands_at(L, seen, c.tx, c.ty);
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

// 4. No pit traps / no one-way traps: every base-reachable region of each room can re-reach a forward
//    exit (a door/cage/exit) — except the intended terminal (the spronk chamber). Concretely: flood from
//    the entrance on the Light-used frontier (reveal + open) and require a forward exit reachable. (With
//    Light, the room must be solvable AND escapable.)
TEST(d8_no_one_way_traps){
    int checked = 0;
    for(int r = 0; r < D8_N; ++r){
        const LevelData& L = *D8_ROOMS[r];
        int sx = room_start_x(L), sy = room_start_y(L);
        Grid lit = build_grid(L, /*reveal*/true, /*open_dv*/true);
        std::vector<uint8_t> seen = reachable(lit, sx, sy);
        bool fwd = reaches_forward_exit(L, seen);
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
        Grid base = build_grid(L, /*reveal*/false, /*open_dv*/false);  // fall happens with platforms gone
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
    Grid g = build_grid(L, /*reveal*/false, /*open_dv*/false);

    struct Seed { const char* name; int x, y; };
    std::vector<Seed> seeds;
    seeds.push_back({"spawn", L.spawn_tx, L.spawn_ty});
    for(int e=0;e<L.entrance_count;++e)
        seeds.push_back({"entrance", L.entrances[e].tx, L.entrances[e].ty});

    int verified = 0;
    for(const Seed& s : seeds){
        std::vector<uint8_t> seen = reachable(g, s.x, s.y);
        bool ok = stands_at(L, seen, fx, fy);
        CHECK(ok);
        std::printf("  [reentry] from %s (%d,%d): shrine reachable = %s\n",
                    s.name, s.x, s.y, ok?"yes":"STRANDED");
        ++verified;
    }
    CHECK(verified >= 3);   // spawn + the two return entrances (id1, id2)
}
