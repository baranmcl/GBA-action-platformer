#pragma once
// -----------------------------------------------------------------------------
// level_harness.h — the ONE shared gate-aware reachability model for host tests.
//
// This retires the six divergent per-dungeon flood-fill forks (test_dungeon{1,2,3,
// 7,8,9}_level.cpp) into a single model. Its BFS core is ported from the most-evolved
// copy (test_dungeon8_level.cpp) and its horizontal double-jump from test_dungeon2_level.cpp.
//
// DUAL-THRESHOLD movement (see test_dungeon8_level.cpp lines 27-42 for the shipped
// soft-lock this fixes):
//   * CLIMB_RELIABLE (5) bounds what a player MUST be able to reach (an ordinary,
//     non-pixel-perfect air-jump). Flood must-reach content at RELIABLE.
//   * CLIMB_MAX      (7) bounds what a gate MUST be able to prevent (the edge apex-timed
//     air-jump). Flood must-NOT-bypass checks at MAX (set WorldModel::climb_max).
// The two constants are pinned against the REAL logic::Player physics by the canary in
// test_level_harness.cpp — that canary FAILS (by design) if jump physics are retuned.
//
// Everything is `inline` — all test TUs link into one binary, so inline is ODR-safe.
// This is the single reason the M14 "copy the helpers into each TU" mandate is retired.
// -----------------------------------------------------------------------------
#include "logic/level_data.h"
#include "logic/tilemap.h"
#include "logic/gates.h"
#include "logic/player.h"     // logic::Player / InputFrame / Body (settle invariant)
#include "test_framework.h"   // CHECK / REQUIRE (the void universal invariants)
#include <vector>
#include <queue>
#include <set>
#include <utility>
#include <cstdint>
#include <cstdio>

namespace harness {

// Movement constants — see test_dungeon8_level.cpp history: RELIABLE for must-reach,
// MAX for must-NOT-bypass. Canary-pinned against player physics in test_level_harness.cpp.
inline constexpr int CLIMB_RELIABLE = 5;
inline constexpr int CLIMB_MAX      = 7;

// Player body in tiles: 2 wide x 4 tall (16x32 px). Mirrors every per-dungeon fork.
inline constexpr int PW = 2;
inline constexpr int PH = 4;

struct WorldModel {
    // Which gate indices are OPEN (cleared/latched) for this query.
    std::set<int> open_gates;            // index into level.gates
    std::set<int> open_columns;          // extra opened trigger-target columns (plate/button/brazier targets)
    bool hidden_platforms_shown = false; // Light reveal active
    bool water_frozen = false;           // Ice bridges cast on every water run
    bool boulders_broken = false;        // Stone pound cleared the boulders (D7 progression states)
    bool cracked_floors_broken = false;  // Stone pound smashed the cracked floors
    bool loose_dropped = false;          // Stone pound released the loose platforms to their rest row
    bool climb_max = false;              // use CLIMB_MAX instead of CLIMB_RELIABLE (bypass checks)
    // Task 2.6 adds: bool glide, bool grapple (wind/updraft/anchor rules)
};

struct Grid {
    int w = 0, h = 0;
    std::vector<uint8_t> solid;     // 1 = blocked for movement
    std::vector<uint8_t> standable; // 1 = feet can rest (solid or one-way top)
    std::vector<uint8_t> hazard;    // lava/water(unfrozen)/spikes

    // Out-of-bounds is treated as solid/standable (a wall) and non-hazard, exactly as the
    // per-dungeon forks did with blk()/standable().
    bool blk(int x, int y) const { if(x<0||y<0||x>=w||y>=h) return true;  return solid[y*w+x]!=0; }
    bool haz(int x, int y) const { if(x<0||y<0||x>=w||y>=h) return false; return hazard[y*w+x]!=0; }
    bool surf(int x, int y) const{ if(x<0||y<0||x>=w||y>=h) return true;  return standable[y*w+x]!=0; }
    bool cell_clear(int x, int y) const { return !blk(x,y) && !haz(x,y); }
    // The 2x4 body fits at feet-position (x,y) (occupies rows y-PH+1..y, cols x..x+PW-1).
    bool fits(int x, int y) const {
        if(x<0 || x+PW-1>=w || y-PH+1<0 || y>=h) return false;
        for(int cx=x; cx<x+PW; ++cx)
            for(int cy=y-PH+1; cy<=y; ++cy)
                if(!cell_clear(cx,cy)) return false;
        return true;
    }
    // Standing = the body fits AND at least one foot column rests on a standable surface below.
    bool stand(int x, int y) const { return fits(x,y) && (surf(x,y+1) || surf(x+PW-1,y+1)); }
};

// -----------------------------------------------------------------------------
// build_grid — the movement grid for a room under a given world model.
// Encodes: solid tiles, one-ways (standable but non-blocking), closed gates as full-height
// 2-wide columns (rows 1..h-3, mirroring scene_dungeon.cpp fill_column), cracked floors solid
// unless broken, blocks solid at spawn, boulders solid unless broken, loose platforms solid at
// spawn row, hidden platforms solid iff shown, water hazard iff not frozen (frozen = standable
// IcePlatform run, non-hazard). open_gates are NOT filled; open_columns are force-cleared.
// -----------------------------------------------------------------------------
inline Grid build_grid(const logic::LevelData& L, const WorldModel& wm){
    Grid g; g.w = L.w; g.h = L.h;
    const int n = L.w * L.h;
    g.solid.assign(n, 0);
    g.standable.assign(n, 0);
    g.hazard.assign(n, 0);
    std::vector<uint8_t> oneway(n, 0);
    auto idx = [&](int x, int y){ return y*L.w + x; };

    // 1. Base tiles.
    for(int y=0; y<L.h; ++y) for(int x=0; x<L.w; ++x){
        switch((logic::TileKind)L.tiles[idx(x,y)]){
            case logic::TileKind::Solid:
            case logic::TileKind::IcePlatform: g.solid[idx(x,y)] = 1; break;
            case logic::TileKind::OneWay:      oneway[idx(x,y)] = 1; break;
            case logic::TileKind::Lava:
            case logic::TileKind::Spikes:      g.hazard[idx(x,y)] = 1; break;
            case logic::TileKind::Water:
                if(wm.water_frozen) g.solid[idx(x,y)] = 1;   // frozen = standable IcePlatform run
                else                g.hazard[idx(x,y)] = 1;  // damaging hazard
                break;
            default: break;
        }
    }
    // 2. Gates. CrackedFloor is a single SOLID floor tile (not a wall column); every other gate,
    //    when NOT open, fills a full-height 2-wide column exactly like scene_dungeon.cpp does for
    //    both obstacle AND unowned-geometry gates.
    for(int i=0; i<L.gate_count; ++i){
        const logic::GateSpawn& gt = L.gates[i];
        if(gt.type == logic::GateType::CrackedFloor){
            if(!wm.cracked_floors_broken && gt.tx>=0 && gt.tx<L.w && gt.ty>=0 && gt.ty<L.h)
                g.solid[idx(gt.tx, gt.ty)] = 1;
            continue;
        }
        if(wm.open_gates.count(i)) continue;             // owned ability / latched shortcut -> open
        for(int ty=1; ty<L.h-2; ++ty) for(int dx=0; dx<2; ++dx){
            int x = gt.tx + dx; if(x>=0 && x<L.w) g.solid[idx(x,ty)] = 1;
        }
    }
    // 3. Blocks — solid at spawn.
    for(int i=0; i<L.block_count; ++i){
        const logic::BlockSpawn& b = L.blocks[i];
        if(b.tx>=0 && b.tx<L.w && b.ty>=0 && b.ty<L.h) g.solid[idx(b.tx,b.ty)] = 1;
    }
    // 4. Boulders — solid unless a pound cleared them.
    if(!wm.boulders_broken)
        for(int i=0; i<L.boulder_count; ++i){
            const logic::BoulderSpawn& b = L.boulders[i];
            if(b.tx>=0 && b.tx<L.w && b.ty>=0 && b.ty<L.h) g.solid[idx(b.tx,b.ty)] = 1;
        }
    // 5. Loose platforms — solid at their authored (load-state) row, OR dropped to their rest row when a
    //    Stone pound has released them (wm.loose_dropped): the first row whose tile directly below any
    //    column of the run is solid (mirrors the scene / the retired D7 fork's build_grid_stone_cleared).
    //    The scan sees the grid built so far (base tiles + gates + broken/unbroken boulders), so a run
    //    falls through hazard tiles (spikes/lava/water) and rests on the first solid below — e.g. bridging
    //    a spike pit on the bottom floor.
    for(int i=0; i<L.loose_platform_count; ++i){
        const logic::LoosePlatformSpawn& lp = L.loose_platforms[i];
        int ty = lp.ty;
        if(wm.loose_dropped){
            while(ty+1 < L.h){
                bool rest = false;
                for(int dx=0; dx<lp.len; ++dx){ int x=lp.tx+dx; if(x>=0 && x<L.w && g.solid[idx(x,ty+1)]) rest = true; }
                if(rest) break; ++ty;
            }
        }
        for(int dx=0; dx<lp.len; ++dx){
            int x = lp.tx+dx; if(x>=0 && x<L.w && ty>=0 && ty<L.h) g.solid[idx(x,ty)] = 1;
        }
    }
    // 6. Hidden platforms — solid iff a Light reveal is active.
    if(wm.hidden_platforms_shown)
        for(int i=0; i<L.hidden_platform_count; ++i){
            const logic::HiddenPlatformSpawn& hp = L.hidden_platforms[i];
            for(int dx=0; dx<hp.len; ++dx){
                int x = hp.tx+dx; if(x>=0 && x<L.w && hp.ty>=0 && hp.ty<L.h) g.solid[idx(x,hp.ty)] = 1;
            }
        }
    // 7. Force-open trigger-target columns (a plate/button/brazier cleared this 2-wide wall).
    for(int tx : wm.open_columns)
        for(int ty=1; ty<L.h-2; ++ty) for(int dx=0; dx<2; ++dx){
            int x = tx+dx; if(x>=0 && x<L.w) g.solid[idx(x,ty)] = 0;
        }
    // 8. Standable = solid OR one-way top (built last so it reflects opened columns).
    for(int i=0; i<n; ++i) g.standable[i] = g.solid[i] || oneway[i];
    return g;
}

// -----------------------------------------------------------------------------
// BFS core (ported from test_dungeon8_level.cpp) + horizontal double-jump (test_dungeon2_level.cpp).
// -----------------------------------------------------------------------------
namespace detail {

// Snap a seed down to the first standing position at/below (sx,sy) (mirrors the fork snap_start).
inline bool snap_start(const Grid& g, int& sx, int& sy){
    for(int lx : { sx, sx-1 }){
        int y = sy;
        while(y < g.h){
            if(g.stand(lx, y)){ sx = lx; sy = y; return true; }
            if(g.fits(lx, y) && !(g.surf(lx,y+1) || g.surf(lx+PW-1,y+1))){ ++y; continue; }
            ++y;
        }
    }
    return false;
}

// Flood standable feet-positions reachable from (sx,sy) with a given climb reach.
// Neighbour rules: walk, fall any height, climb (double-jump) up to `climb` tiles straight
// or diagonally, and horizontal double-jump over gaps <= climb-1 tiles at the same landing row.
inline std::vector<uint8_t> flood(const Grid& g, int sx, int sy, int climb){
    std::vector<uint8_t> seen(g.w*g.h, 0);
    if(!snap_start(g, sx, sy)) return seen;
    std::queue<std::pair<int,int>> q;
    auto push = [&](int x, int y){
        if(x<0 || y<0 || x>=g.w || y>=g.h) return;
        if(!g.stand(x,y) || seen[y*g.w+x]) return;
        seen[y*g.w+x] = 1; q.push({x,y});
    };
    push(sx, sy);
    while(!q.empty()){
        auto pr = q.front(); q.pop();
        int x = pr.first, y = pr.second;
        // straight-up climb
        for(int up=1; up<=climb; ++up){
            int ny = y - up;
            if(ny - PH + 1 < 0) break;
            if(!g.fits(x, ny)) break;
            if(g.stand(x, ny)) push(x, ny);
        }
        // diagonal climb + sideways-step-then-fall
        for(int dir=-1; dir<=1; dir+=2){
            int nx = x + dir;
            for(int up=1; up<=climb; ++up){
                int ny = y - up;
                if(ny - PH + 1 < 0) break;
                if(!g.fits(x, ny)) break;   // gated on the START column's head clearance (faithful port)
                if(g.stand(nx, ny)) push(nx, ny);
            }
            if(g.fits(nx, y)){
                int ny = y;
                while(ny+1 < g.h && g.fits(nx, ny+1) && !(g.surf(nx,ny+1) || g.surf(nx+PW-1,ny+1))) ++ny;
                if(g.stand(nx, ny)) push(nx, ny);
                else if(g.stand(nx, y)) push(nx, y);
            }
        }
        // horizontal double-jump OVER a floor-level hazard/gap (the D2 move): land at the same
        // row up to `climb` tiles away, provided the head-room corridor between is solid-free.
        for(int dir=-1; dir<=1; dir+=2){
            for(int k=2; k<=climb; ++k){
                int nx = x + dir*k;
                if(nx<0 || nx+PW-1>=g.w) break;
                if(!g.stand(nx, y)) continue;
                int c0 = (dir<0) ? nx    : x;
                int c1 = (dir<0) ? x+PW-1 : nx+PW-1;
                bool corridor = true;
                for(int cx=c0; cx<=c1 && corridor; ++cx)
                    for(int cy=y-PH+1; cy<=y-1; ++cy)
                        if(g.blk(cx, cy)){ corridor = false; break; }
                if(corridor) push(nx, y);
            }
        }
    }
    return seen;
}

} // namespace detail

inline int climb_of(const WorldModel& wm){ return wm.climb_max ? CLIMB_MAX : CLIMB_RELIABLE; }
inline int room_start_x(const logic::LevelData& L){ return L.entrance_count ? L.entrances[0].tx : L.spawn_tx; }
inline int room_start_y(const logic::LevelData& L){ return L.entrance_count ? L.entrances[0].ty : L.spawn_ty; }

// Flood of standable feet-positions from a given seed tile.
inline std::set<std::pair<int,int>> reachable_from(const logic::LevelData& level, const WorldModel& wm, int tx, int ty){
    Grid g = build_grid(level, wm);
    std::vector<uint8_t> seen = detail::flood(g, tx, ty, climb_of(wm));
    std::set<std::pair<int,int>> out;
    for(int y=0; y<g.h; ++y) for(int x=0; x<g.w; ++x) if(seen[y*g.w+x]) out.insert({x,y});
    return out;
}
// Flood from the room's start (entrance 0 if present, else the raw spawn).
inline std::set<std::pair<int,int>> reachable(const logic::LevelData& level, const WorldModel& wm){
    return reachable_from(level, wm, room_start_x(level), room_start_y(level));
}

// Does a reachable feet-position put the player AT content tile (tx,ty)? (feet at ty or ty+1,
// within the 2-wide body's columns tx-PW+1..tx — mirrors every fork's stands_at().)
inline bool reaches(const logic::LevelData& level, const WorldModel& wm, int tx, int ty){
    std::set<std::pair<int,int>> R = reachable(level, wm);
    for(int dy=0; dy<=1; ++dy){
        int fy = ty + dy;
        for(int lx=tx-PW+1; lx<=tx; ++lx)
            if(R.count({lx, fy})) return true;
    }
    return false;
}

// Standable cell nearest-below a content tile (mirrors scene_dungeon.cpp floor_row_below:
// the first solid-or-one-way surface strictly below ty; falls back to ty+1).
inline std::pair<int,int> ground_below(const Grid& g, int tx, int ty){
    for(int y=ty+1; y<g.h; ++y)
        if(tx>=0 && tx<g.w && g.standable[y*g.w+tx]) return {tx, y};
    return {tx, ty+1};
}

// -----------------------------------------------------------------------------
// Universal invariants (for test_all_dungeons.cpp; return void, CHECK inside).
// -----------------------------------------------------------------------------
inline void check_solid_border(const logic::LevelData& level, const char* label){
    const int S = (int)logic::TileKind::Solid;
    for(int x=0; x<level.w; ++x){
        CHECK((int)level.tiles[x] == S);                       // top row
        CHECK((int)level.tiles[(level.h-1)*level.w + x] == S);  // bottom row
    }
    for(int y=0; y<level.h; ++y){
        CHECK((int)level.tiles[y*level.w] == S);                // left col
        CHECK((int)level.tiles[y*level.w + (level.w-1)] == S);  // right col
    }
    std::printf("  [border] %s: %dx%d solid border verified\n", label, level.w, level.h);
}

inline void check_room_doors_resolve(const logic::DungeonData& dungeon, const char* label){
    for(int r=0; r<dungeon.room_count; ++r){
        const logic::LevelData& L = *dungeon.rooms[r];
        for(int i=0; i<L.room_door_count; ++i){
            const logic::RoomDoorSpawn& rd = L.room_doors[i];
            if(rd.target_room < 0) continue;   // hub-exit door
            CHECK(rd.target_room >= 0 && rd.target_room < dungeon.room_count);
            if(rd.target_room < 0 || rd.target_room >= dungeon.room_count) continue;
            const logic::LevelData& T = *dungeon.rooms[rd.target_room];
            bool found = false;
            for(int e=0; e<T.entrance_count; ++e) if(T.entrances[e].id == rd.target_entrance) found = true;
            CHECK(found);   // the target room actually has the entrance this door lands on
        }
    }
    std::printf("  [doors] %s: %d rooms, all room-door targets resolve\n", label, dungeon.room_count);
}

namespace detail {
inline bool body_overlaps_hazard(const logic::Body& b, const logic::Tilemap& m){
    int l  = logic::Tilemap::px_to_tile(b.pos.x);
    int r  = logic::Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - logic::Fixed::from_int(1));
    int t  = logic::Tilemap::px_to_tile(b.pos.y);
    int bm = logic::Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - logic::Fixed::from_int(1));
    for(int ty=t; ty<=bm; ++ty) for(int tx=l; tx<=r; ++tx){
        logic::TileKind k = m.at(tx,ty);
        if(k==logic::TileKind::Lava || k==logic::TileKind::Water || k==logic::TileKind::Spikes) return true;
    }
    return false;
}
} // namespace detail

// The respawn-settle invariant (generalised from test_d1_boss_respawn.cpp): a 4-tall logic::Player
// body dropped at each entrance (tx*8,ty*8) must, after a 60-frame settle, end on_ground and NOT
// overlap any hazard tile. Guards fall-through / spawned-in-lava soft-locks.
inline void check_entrances_settle_safely(const logic::LevelData& level, const char* label){
    logic::Tilemap map{ level.w, level.h, level.tiles };
    for(int e=0; e<level.entrance_count; ++e){
        int ex = level.entrances[e].tx, ey = level.entrances[e].ty;
        logic::Player p;
        p.body.half_w = logic::Fixed::from_int(8);
        p.body.half_h = logic::Fixed::from_int(16);
        p.body.pos = { logic::Fixed::from_int(ex*8), logic::Fixed::from_int(ey*8) };
        p.body.vel = { logic::Fixed::from_int(0), logic::Fixed::from_int(0) };
        p.body.on_ground = false;
        logic::InputFrame in{};
        for(int f=0; f<60; ++f) p.update(in, map);
        bool hz = detail::body_overlaps_hazard(p.body, map);
        CHECK(p.body.on_ground);
        CHECK(!hz);
        std::printf("  [settle] %s entrance id=%d (%d,%d): on_ground=%d hazard=%d\n",
                    label, level.entrances[e].id, ex, ey, (int)p.body.on_ground, (int)hz);
    }
}

} // namespace harness
