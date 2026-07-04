#include "test_framework.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon3_room0.h"
#include "game/levels/dungeon3_room1.h"
#include "game/levels/dungeon3_room2.h"
#include <vector>
#include <queue>
#include <cstdio>
using namespace logic;

// Frost Hollow (Dungeon 3) — M14 restructure into 3 rooms (puzzle -> Coldforge Twins boss arena ->
// spronk). Standard structural invariants PLUS first-class no-soft-lock invariants that each FAIL
// on a deliberately-broken layout (verified during authoring; see the phase report). All assert
// against the COMPILED DUNGEON3_ROOM* data.
//
// The flood-fill harness is the SAME 2-wide x 4-tall body model used for D1/D2, with BASE
// movement: bolt + Fire (needed to counter the ice head). Rooms 1-2 are flat-floor arenas with
// no hidden platforms/dark veils/one-way tiles, so one static grid suffices.

static const LevelData* const D3_ROOMS[] = {
    &DUNGEON3_ROOM0_DATA, &DUNGEON3_ROOM1_DATA, &DUNGEON3_ROOM2_DATA };
static constexpr int D3_N = 3;

// Generous double-jump reach — same as D1/D2.
static constexpr int CLIMB = 5;

static constexpr int PW = 2;   // player width in tiles
static constexpr int PH = 4;   // player height in tiles

struct D3Grid {
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

static D3Grid build_grid(const LevelData& L){
    D3Grid g; g.w = L.w; g.h = L.h;
    g.solid.assign(L.w*L.h, 0);
    g.hazard.assign(L.w*L.h, 0);
    for(int y = 0; y < L.h; ++y) for(int x = 0; x < L.w; ++x){
        TileKind k = (TileKind)L.tiles[y*L.w + x];
        if(k == TileKind::Solid || k == TileKind::IcePlatform) g.solid[y*L.w + x] = 1;
        if(k == TileKind::Lava || k == TileKind::Water || k == TileKind::Spikes) g.hazard[y*L.w + x] = 1;
    }
    return g;
}

static bool snap_start(const D3Grid& g, int& sx, int& sy){
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

static std::vector<uint8_t> reachable(const D3Grid& g, int sx, int sy, int climb){
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
        // Horizontal double-jump OVER a floor-level hazard/gap.
        for(int dir = -1; dir <= 1; dir += 2){
            for(int k = 2; k <= climb; ++k){
                int nx = x + dir * k;
                if(nx < 0 || nx + PW - 1 >= g.w) break;
                if(!g.standable(nx, y)) continue;
                int c0 = (dir < 0) ? nx : x;
                int c1 = (dir < 0) ? x + PW - 1 : nx + PW - 1;
                bool corridor = true;
                for(int cx = c0; cx <= c1 && corridor; ++cx)
                    for(int cy = y - PH + 1; cy <= y - 1; ++cy)
                        if(g.blk(cx, cy)){ corridor = false; break; }
                if(corridor) push(nx, y);
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

static int d3_tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }

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
TEST(d3_room1_onward_door_reachable){   // fail-on-broken
    const LevelData& L = DUNGEON3_ROOM1_DATA;
    D3Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, room_start_x(L), room_start_y(L), CLIMB);
    bool onward=false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==2 && stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) onward=true;
    CHECK(onward);
    std::printf("  [d3-room1] onward-door(to room2)=%s\n", onward?"reach":"NO");
}

// 2. Room 2 (spronk): from the room-2 entrance BOTH the caged spronk AND the exit are reachable.
//    Break test: float/wall the cage or exit -> RED.
TEST(d3_room2_spronk_and_exit_reachable){
    const LevelData& L = DUNGEON3_ROOM2_DATA;
    D3Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, room_start_x(L), room_start_y(L), CLIMB);
    bool c = stands_at(L, seen, L.cage_tx, L.cage_ty);
    bool e = stands_at(L, seen, L.exit_tx, L.exit_ty);
    CHECK(c); CHECK(e);
    std::printf("  [d3-room2] cage=%s exit=%s\n", c?"reach":"NO", e?"reach":"NO");
}

// 3. Room 0 is a spell-gated puzzle (not flood-fill-traversable): assert the hub-return is reachable
//    from spawn + the onward door exists (the accepted D2 deviation).
TEST(d3_room0_hub_return_reachable_and_onward_exists){
    const LevelData& L = DUNGEON3_ROOM0_DATA;
    D3Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, L.spawn_tx, L.spawn_ty, CLIMB);
    bool hub=false, onward=false;
    for(int i=0;i<L.room_door_count;++i){
        if(L.room_doors[i].target_room==-1 && stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) hub=true;
        if(L.room_doors[i].target_room==1) onward=true;   // structural existence
    }
    CHECK(hub); CHECK(onward);
    std::printf("  [d3-room0] hub-return=%s onward-exists=%s\n", hub?"reach":"NO", onward?"yes":"NO");
}
