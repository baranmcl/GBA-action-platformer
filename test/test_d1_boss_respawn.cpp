#include "test_framework.h"
#include "logic/player.h"
#include "logic/collision.h"
#include "game/levels/dungeon1_room0.h"
#include "game/levels/dungeon1_room1.h"
#include "game/levels/dungeon1_room2.h"
using namespace logic;

// M12 QA r2 — the D1 boss arena (room 1) respawn must settle the player ON the floor, not let them
// fall through. A death in the post-boss room loop respawns at spawn_pos = entrance 0 (the room-1
// arrival/return door). `Body::pos` is the TOP-LEFT corner, and the entrance's ty is the *feet* row,
// so spawning the 4-tall body at (ex*8, ey*8) embeds it ~3 rows into the floor; move_and_collide must
// back it out UPWARD to a standing rest. If it ever resolves downward / out-of-bounds, the player
// "vanishes and falls with no menu" — the QA "died on spikes, no respawn, soft-lock" report.
//
// This drives the REAL physics (player.update -> apply_gravity + move_and_collide) against the
// COMPILED DUNGEON1_ROOM1_DATA tilemap, from the EXACT respawn coordinates.

static bool body_overlaps_solid(const Body& b, const Tilemap& m){
    int l = Tilemap::px_to_tile(b.pos.x);
    int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
    int t = Tilemap::px_to_tile(b.pos.y);
    int bm= Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - Fixed::from_int(1));
    for(int ty=t; ty<=bm; ++ty) for(int tx=l; tx<=r; ++tx) if(m.is_solid(tx,ty)) return true;
    return false;
}

TEST(d1_room1_respawn_settles_on_floor){
    const LevelData& L = DUNGEON1_ROOM1_DATA;
    Tilemap map{ L.w, L.h, L.tiles };
    // entrance 0 — the room-1 arrival/return door, where a death in the post-boss loop respawns.
    int ex = L.entrances[0].tx, ey = L.entrances[0].ty;

    Player player;
    player.body.half_w = Fixed::from_int(8); player.body.half_h = Fixed::from_int(16);
    player.body.pos = { Fixed::from_int(ex*8), Fixed::from_int(ey*8) };   // EXACT respawn position
    player.body.vel = { Fixed::from_int(0), Fixed::from_int(0) };
    player.body.on_ground = false;

    InputFrame in{};
    for(int f=0; f<60; ++f) player.update(in, map);

    int top_row  = Tilemap::px_to_tile(player.body.pos.y);
    int feet_row = Tilemap::px_to_tile(player.body.pos.y + player.body.half_h + player.body.half_h - Fixed::from_int(1));
    bool buried  = body_overlaps_solid(player.body, map);
    std::printf("  [d1-respawn] entrance=(%d,%d) settled top-left y=%d top_row=%d feet_row=%d on_ground=%d buried=%d\n",
                ex, ey, player.body.pos.y.to_int(), top_row, feet_row, (int)player.body.on_ground, (int)buried);

    CHECK(player.body.on_ground);          // must end grounded (not falling forever)
    CHECK(!buried);                        // body must not overlap any solid tile (not embedded/fallen)
    CHECK(feet_row < L.h);                 // feet inside the room, not below the bottom border
}

// Generalize: EVERY entrance of EVERY D1 room must settle a respawning body to a grounded, non-buried
// rest. The dungeon's death-respawn and door-transition both place the body at an entrance's (tx*8,
// ty*8), so any embedded entrance would be a fall-through soft-lock. Drive the real physics from each.
static void check_entrance_settles(const LevelData& L, const char* room){
    Tilemap map{ L.w, L.h, L.tiles };
    for(int e = 0; e < L.entrance_count; ++e){
        int ex = L.entrances[e].tx, ey = L.entrances[e].ty;
        Player p;
        p.body.half_w = Fixed::from_int(8); p.body.half_h = Fixed::from_int(16);
        p.body.pos = { Fixed::from_int(ex*8), Fixed::from_int(ey*8) };
        p.body.vel = { Fixed::from_int(0), Fixed::from_int(0) };
        p.body.on_ground = false;
        InputFrame in{};
        for(int f=0; f<60; ++f) p.update(in, map);
        int feet_row = Tilemap::px_to_tile(p.body.pos.y + p.body.half_h + p.body.half_h - Fixed::from_int(1));
        bool buried = body_overlaps_solid(p.body, map);
        std::printf("  [d1-spawn] %s entrance id=%d at (%d,%d): feet_row=%d on_ground=%d buried=%d\n",
                    room, L.entrances[e].id, ex, ey, feet_row, (int)p.body.on_ground, (int)buried);
        CHECK(p.body.on_ground);
        CHECK(!buried);
        CHECK(feet_row < L.h);
    }
}

TEST(d1_all_entrances_settle_on_floor){
    check_entrance_settles(DUNGEON1_ROOM0_DATA, "room0");
    check_entrance_settles(DUNGEON1_ROOM1_DATA, "room1");
    check_entrance_settles(DUNGEON1_ROOM2_DATA, "room2");
}
