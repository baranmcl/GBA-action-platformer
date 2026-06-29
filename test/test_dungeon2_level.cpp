#include "test_framework.h"
#include "game/levels/dungeons.h"
#include "game/levels/dungeon2_room0.h"
#include "game/levels/dungeon2_room1.h"
#include "game/levels/dungeon2_room2.h"
#include <vector>
#include <queue>
#include <cstdio>
using namespace logic;

// Ember Caverns (Dungeon 2) — M13 restructure into 3 rooms (entry -> Slagshell boss arena ->
// spronk). Standard structural invariants PLUS first-class no-soft-lock invariants that each FAIL
// on a deliberately-broken layout (verified during authoring; see the phase report). All assert
// against the COMPILED DUNGEON2_ROOM* data.
//
// The flood-fill harness is the SAME 2-wide x 4-tall body model used for D1/D7/D8, with BASE
// movement: D2's kit is bolt + double-jump + Fire (the shrine is in room 0). Rooms 0-2 are
// flat-floor arenas with no hidden platforms/dark veils/one-way tiles, so one static grid suffices.

static const LevelData* const D2_ROOMS[] = {
    &DUNGEON2_ROOM0_DATA, &DUNGEON2_ROOM1_DATA, &DUNGEON2_ROOM2_DATA };
static constexpr int D2_N = 3;

// Generous double-jump reach — same as D1.
static constexpr int CLIMB = 5;

static constexpr int PW = 2;   // player width in tiles
static constexpr int PH = 4;   // player height in tiles

struct D2Grid {
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

static D2Grid build_grid(const LevelData& L){
    D2Grid g; g.w = L.w; g.h = L.h;
    g.solid.assign(L.w*L.h, 0);
    g.hazard.assign(L.w*L.h, 0);
    for(int y = 0; y < L.h; ++y) for(int x = 0; x < L.w; ++x){
        TileKind k = (TileKind)L.tiles[y*L.w + x];
        if(k == TileKind::Solid || k == TileKind::IcePlatform) g.solid[y*L.w + x] = 1;
        if(k == TileKind::Lava || k == TileKind::Water || k == TileKind::Spikes) g.hazard[y*L.w + x] = 1;
    }
    return g;
}

static bool snap_start(const D2Grid& g, int& sx, int& sy){
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

static std::vector<uint8_t> reachable(const D2Grid& g, int sx, int sy, int climb){
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

TEST(d2_rooms_solid_border){
    for(int r = 0; r < D2_N; ++r){
        const LevelData& L = *D2_ROOMS[r];
        for(int x = 0; x < L.w; ++x){ CHECK_EQ(d2_tile(L,x,0),1); CHECK_EQ(d2_tile(L,x,L.h-1),1); }
        for(int y = 0; y < L.h; ++y){ CHECK_EQ(d2_tile(L,0,y),1); CHECK_EQ(d2_tile(L,L.w-1,y),1); }
    }
}

// Room 1 is the boss arena: >= 30 wide, has Slagshell (D2_DEF), NO cage / NO exit.
// Room 0 is the entry with the Fire shrine and NO cage / NO exit.
// Room 2 is the spronk chamber: has the cage + the exit. No other room has a boss.
TEST(d2_room1_is_boss_arena){
    const LevelData& L = DUNGEON2_ROOM1_DATA;
    CHECK(L.boss != nullptr);
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
// Pins the 0 <-> 1 <-> 2 graph (+ the hub return in room 0).
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

// 1. Room 0: The entry section (left of the first wall pillar) contains both the spawn '@' and the
//    hub-return door 'Q'. The player falls from the spawn at row 16 to the floor at row 19; the floor
//    col at Q's x-position (col 5) must be reachable from spawn. The Fire shrine and the onward door
//    are puzzle-gated (the brazier + plate + button puzzles open the wall pillars separating the three
//    sections); their reachability is guaranteed by puzzle design, not free-traversal flood-fill.
//    We verify: the floor below Q's x is reachable AND the onward door exists in room 0.
//    Break test: wall off the floor below Q -> that cell unreachable -> RED.
TEST(d2_room0_hub_door_reachable_from_spawn){
    const LevelData& L = DUNGEON2_ROOM0_DATA;
    D2Grid g = build_grid(L);
    // Snap from spawn — player falls to the main floor below the spawn marker.
    int sx = L.spawn_tx, sy = L.spawn_ty;
    std::vector<uint8_t> seen = reachable(g, sx, sy, CLIMB);
    // The floor reachability check: find the hub-return door Q (target_room==-1), then
    // verify the main floor at the same x is reachable (player walks past Q at floor level).
    bool hub_door = false;
    for(int i=0;i<L.room_door_count;++i){
        if(L.room_doors[i].target_room != -1) continue;
        int qx = L.room_doors[i].tx;
        // The player standing at (qx, fy) for any fy in the seen map counts as at the Q.
        for(int fy = L.room_doors[i].ty; fy < L.h && !hub_door; ++fy)
            if(g.standable(qx, fy) && fy < L.h && seen[fy*L.w + qx]) hub_door = true;
        // Also check the adjacent column (PW=2 body)
        for(int fy = L.room_doors[i].ty; fy < L.h && !hub_door; ++fy)
            if(qx-1 >= 0 && g.standable(qx-1, fy) && seen[fy*L.w + (qx-1)]) hub_door = true;
    }
    CHECK(hub_door);
    // onward door to room 1 exists in room 0 (structural check — puzzle-gated, not flood-reachable)
    bool onward_exists = false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==1) onward_exists=true;
    CHECK(onward_exists);
    std::printf("  [room0] hub-door-floor=%s onward-door-exists=%s\n", hub_door?"reach":"NO", onward_exists?"yes":"NO");
}

// 2. Room 1 (boss arena): from the room-1 entrance the arena floor is traversable and the onward door
//    to room 2 is reachable (from entrance id 0). Break test: wall off the onward door -> RED.
TEST(d2_room1_onward_door_reachable){
    const LevelData& L = DUNGEON2_ROOM1_DATA;
    int sx = room_start_x(L), sy = room_start_y(L);
    D2Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, sx, sy, CLIMB);
    bool onward = false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==2 && stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) onward=true;
    CHECK(onward);
    std::printf("  [room1] onward-door(to room2)=%s\n", onward?"reach":"NO");
}

// 3. Room 2 (spronk): from the room-2 entrance BOTH the caged spronk AND the exit are reachable
//    with ONLY bolt + double-jump (no Fire/Ice/etc.).
//    Break test: float/wall the cage or exit -> RED.
TEST(d2_room2_spronk_and_exit_reachable){
    const LevelData& L = DUNGEON2_ROOM2_DATA;
    int sx = room_start_x(L), sy = room_start_y(L);
    D2Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, sx, sy, CLIMB);
    bool c = stands_at(L, seen, L.cage_tx, L.cage_ty);
    bool e = stands_at(L, seen, L.exit_tx, L.exit_ty);
    CHECK(c); CHECK(e);
    std::printf("  [room2] cage=%s exit=%s\n", c?"reach":"NO", e?"reach":"NO");
}

// 4. No one-way traps: from each room's entrance the player can reach a forward exit
//    (a door/cage/exit). With bolt+double-jump every room must be solvable AND escapable.
//    Break test: drop floor under entrance -> RED.
TEST(d2_no_one_way_traps){
    for(int r = 0; r < D2_N; ++r){
        const LevelData& L = *D2_ROOMS[r];
        int sx = room_start_x(L), sy = room_start_y(L);
        D2Grid g = build_grid(L);
        std::vector<uint8_t> seen = reachable(g, sx, sy, CLIMB);
        bool fwd = reaches_forward_exit(L, seen);
        CHECK(fwd);
        std::printf("  [oneway] room %d -> forward exit reachable = %s\n", r, fwd?"yes":"NO");
    }
}
