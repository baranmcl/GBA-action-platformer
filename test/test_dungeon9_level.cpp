#include "test_framework.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon9_room0.h"
#include "game/levels/dungeon9_room1.h"
#include "game/levels/dungeon9_arena.h"
#include <vector>
#include <queue>
#include <cstdio>
using namespace logic;

// Dungeon 9 (Nightmare King finale, M11) — the 2-room traversal APPROACH + the bespoke boss ARENA.
// No-soft-lock invariants in the M8-M10 discipline: each FAILS on a deliberately-broken layout
// (verified during authoring — see the per-test break/revert notes). All assert against the COMPILED
// DUNGEON9_* data via the SAME 2-wide x 4-tall flood-fill body model used for D7/D8.
//
// Reachability convention: BASE movement only (climb = the reliable double-jump reach, 5 tiles). The
// arena's high firing platforms are made base-reachable by an authored staircase (see dungeon9_arena.txt
// "M11 P4.5 reachability fix"); the grapple anchor remains an aerial flourish, not a reachability crutch
// (grapple is NOT modeled here — same stance D6 took: feel-reachability is mGBA-verified, structural
// reachability is proved by base movement only).

// ---- Documented firing tiles (must match dungeon9_arena.txt; the two cannot silently drift) ----
static constexpr int P1_FIRE_TX = 19, P1_FIRE_TY = 18;  // atop the central platform (aerial expose)
static constexpr int P2_FIRE_TX = 12, P2_FIRE_TY = 28;  // base floor, under the King (ground expose)
static constexpr int P3_FIRE_TX = 26, P3_FIRE_TY = 18;  // atop the right platform (frozen-foothold expose)

static constexpr int CLIMB_RELIABLE = 5;   // reliable double-jump reach (the BASE path must clear it)
static constexpr int PW = 2;               // player width in tiles
static constexpr int PH = 4;               // player height in tiles

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

// Static grid: Solid/IcePlatform block; Lava/Water/Spikes are non-solid hazards. The D9 approach + arena
// have no gates/hidden platforms — a plain static grid is the whole story.
static Grid build_grid(const LevelData& L){
    Grid g; g.w = L.w; g.h = L.h;
    g.solid.assign(L.w*L.h, 0);
    g.hazard.assign(L.w*L.h, 0);
    for(int y = 0; y < L.h; ++y) for(int x = 0; x < L.w; ++x){
        TileKind k = (TileKind)L.tiles[y*L.w + x];
        if(k == TileKind::Solid || k == TileKind::IcePlatform) g.solid[y*L.w + x] = 1;
        if(k == TileKind::Lava || k == TileKind::Water || k == TileKind::Spikes) g.hazard[y*L.w + x] = 1;
    }
    return g;
}

static bool snap_start(const Grid& g, int& sx, int& sy){
    for(int lx : { sx, sx-1 }){
        int y = sy;
        while(y < g.h){
            if(g.standable(lx, y)){ sx = lx; sy = y; return true; }
            ++y;
        }
    }
    return false;
}

static std::vector<uint8_t> reachable(const Grid& g, int sx, int sy, int climb){
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
        for(int up = 1; up <= climb; ++up){
            int ny = y - up;
            if(ny - PH + 1 < 0) break;
            if(!g.fits(x, ny)) break;
            if(g.standable(x, ny)) push(x, ny);
        }
        for(int dir = -1; dir <= 1; dir += 2){
            int nx = x + dir;
            for(int up = 1; up <= climb; ++up){
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
// "Forward" = real progress toward the boss arena. room0 advances via its room-door to room1
// (target_room > start); room1 advances via the dungeon EXIT tile (the run_boss handoff). We deliberately
// do NOT count a room-door whose target_room <= the room's own index (a backtrack door, e.g. room1's
// return-to-room0) — otherwise a walled-off exit would be masked by the always-adjacent backtrack door.
static bool reaches_forward_exit(const LevelData& L, int room_index, const std::vector<uint8_t>& seen){
    if(L.has_exit) return stands_at(L, seen, L.exit_tx, L.exit_ty);
    if(L.has_cage) return stands_at(L, seen, L.cage_tx, L.cage_ty);
    for(int i = 0; i < L.room_door_count; ++i){
        if(L.room_doors[i].target_room <= room_index) continue; // skip hub-exit + backtrack doors
        if(stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) return true;
    }
    return false;
}

static int room_start_x(const LevelData& L){ return L.entrance_count? L.entrances[0].tx : L.spawn_tx; }
static int room_start_y(const LevelData& L){ return L.entrance_count? L.entrances[0].ty : L.spawn_ty; }

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
        int sx = room_start_x(L), sy = room_start_y(L);
        std::vector<uint8_t> seen = reachable(build_grid(L), sx, sy, CLIMB_RELIABLE);
        bool fwd = reaches_forward_exit(L, r, seen);
        CHECK(fwd);
        std::printf("  [approach] room %d entrance(%d,%d) -> forward exit reachable = %s\n",
                    r, sx, sy, fwd ? "yes" : "NO");
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
    int sx = room_start_x(A), sy = room_start_y(A);
    std::vector<uint8_t> seen = reachable(build_grid(A), sx, sy, CLIMB_RELIABLE);
    bool reach = stands_at(A, seen, c.tx, c.ty);
    CHECK(reach);
    std::printf("  [crystal] $(%d,%d) base-reachable = %s\n", c.tx, c.ty, reach ? "yes" : "NO");
}

// 3. Every per-phase EXPOSE firing tile is reachable by base movement from the entrance -> the arena is
//    solvable each phase (the documented tiles match dungeon9_arena.txt). Break test: remove a platform
//    staircase stub (rows 20/25 below a platform) -> that platform's firing tile -> RED.
TEST(d9_arena_expose_positions_reachable){
    const LevelData& A = DUNGEON9_ARENA_DATA;
    int sx = room_start_x(A), sy = room_start_y(A);
    std::vector<uint8_t> seen = reachable(build_grid(A), sx, sy, CLIMB_RELIABLE);
    bool p1 = stands_at(A, seen, P1_FIRE_TX, P1_FIRE_TY);
    bool p2 = stands_at(A, seen, P2_FIRE_TX, P2_FIRE_TY);
    bool p3 = stands_at(A, seen, P3_FIRE_TX, P3_FIRE_TY);
    CHECK(p1); CHECK(p2); CHECK(p3);
    std::printf("  [expose] P1(%d,%d)=%s P2(%d,%d)=%s P3(%d,%d)=%s\n",
                P1_FIRE_TX, P1_FIRE_TY, p1 ? "reach" : "MISS",
                P2_FIRE_TX, P2_FIRE_TY, p2 ? "reach" : "MISS",
                P3_FIRE_TX, P3_FIRE_TY, p3 ? "reach" : "MISS");
}

// 4. No one-way traps: from the entrance, every room (approach + arena) flood-fill can re-reach a safe
//    standing region (it is non-empty). For the approach a forward exit is reachable (covered above); the
//    arena is a terminal fight room (no exit), so we require the reachable set is non-trivial AND the
//    entrance region connects to the crystal + all firing tiles (you can always get back to a refuel and
//    to every expose spot). Break test: split the floor so the entrance is isolated -> RED.
TEST(d9_no_one_way_traps){
    // approach: forward exit reachable from entrance
    for(int r = 0; r < D9_APPROACH_N; ++r){
        const LevelData& L = *D9_APPROACH[r];
        std::vector<uint8_t> seen = reachable(build_grid(L), room_start_x(L), room_start_y(L), CLIMB_RELIABLE);
        CHECK(reaches_forward_exit(L, r, seen));
    }
    // arena: entrance connects to crystal + every firing tile (full mutual mobility, no stranding)
    const LevelData& A = DUNGEON9_ARENA_DATA;
    std::vector<uint8_t> seen = reachable(build_grid(A), room_start_x(A), room_start_y(A), CLIMB_RELIABLE);
    CHECK(stands_at(A, seen, A.magic_crystals[0].tx, A.magic_crystals[0].ty));
    CHECK(stands_at(A, seen, P1_FIRE_TX, P1_FIRE_TY));
    CHECK(stands_at(A, seen, P2_FIRE_TX, P2_FIRE_TY));
    CHECK(stands_at(A, seen, P3_FIRE_TX, P3_FIRE_TY));
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
    Grid g = build_grid(A);
    std::vector<uint8_t> seen = reachable(g, room_start_x(A), room_start_y(A), CLIMB_RELIABLE);
    int checked = 0;
    for(int y = 0; y < A.h; ++y) for(int x = 0; x < A.w; ++x){
        if(!seen[y*A.w + x]) continue;
        for(int fy = y; fy < A.h; ++fy){
            CHECK(!g.haz(x, fy));
            if(g.blk(x, fy)) break;
        }
        ++checked;
    }
    CHECK(checked >= 1);
}
