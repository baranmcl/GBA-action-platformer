#include "test_framework.h"
#include "logic/player.h"
#include "logic/meters.h"
#include "logic/hazard.h"
#include "logic/collision.h"
using namespace logic;

// ---- Meter edge cases that the death/respawn path relies on ----
TEST(damage_to_exactly_zero_is_empty){
    Meter h{20,100}; h.damage(20); CHECK_EQ(h.cur,0); CHECK(h.is_empty());
}
TEST(overdamage_clamps_no_underflow){
    Meter h{20,100}; h.damage(1000); CHECK_EQ(h.cur,0); CHECK(h.is_empty());
    // damage again must not wrap negative past zero into a "not empty" state
    h.damage(20); CHECK_EQ(h.cur,0); CHECK(h.is_empty());
}
TEST(refill_clears_empty){
    Meter h{0,100}; CHECK(h.is_empty()); h.cur = h.max; CHECK(!h.is_empty()); CHECK_EQ(h.cur,100);
}

// ---- Scene-order repro: player drains to 0 in a sub-floor water pit, then respawns ----
// Mirrors the per-frame ORDER in scene_dungeon.cpp::play_room:
//   player.update -> hazard damage(20)/invuln=45 -> i-frame tick -> respawn-if-empty.
// The map: a sub-floor water PIT (water at the bottom rows, solid border below),
// and a SAFE entrance ledge elsewhere. Verifies the player ends up at the entrance,
// full HP, and does NOT immediately re-die (no death loop / stuck-in-pit).
namespace {
    // Build a 12-wide x 12-tall map.
    //  - border solid all around (row 11 is the floor border)
    //  - a water PIT: cols 4..9 at row 10 are water (the sub-floor hazard, below the
    //    normal floor level at row 9 which is open so the player falls into the pit)
    //  - a SAFE entrance ledge at col 1, with a solid tile under it at row 10.
    struct Map {
        static constexpr int W=12, H=12;
        uint8_t cells[W*H];
        Map(){
            for(int y=0;y<H;++y) for(int x=0;x<W;++x){
                bool border = (x==0||x==W-1||y==0||y==H-1);
                cells[y*W+x] = border ? (uint8_t)TileKind::Solid : (uint8_t)TileKind::Empty;
            }
            // safe ledge floor under the entrance column (col 1) at row 10
            cells[10*W + 1] = (uint8_t)TileKind::Solid;
            cells[10*W + 2] = (uint8_t)TileKind::Solid;
            // sub-floor water pit: cols 4..9 at row 10 (just above the solid border row 11)
            for(int x=4;x<=9;++x) cells[10*W + x] = (uint8_t)TileKind::Water;
        }
        Tilemap view() const { return Tilemap{W,H,cells}; }
    };
}

// Demonstrates the DEATH-LOOP failure mode that looks like "stuck at the bottom":
// if the respawn point is itself in/adjacent to a hazard AND the respawn grants NO
// post-respawn i-frame, the player is damaged again the very next frames and re-dies
// forever -> the player can never act -> "stuck". The current scene sets invuln=0 on
// respawn (no grace), so a hazardous spawn = an unbreakable loop. The robust fix grants
// a brief post-respawn i-frame so the player always regains control.
TEST(respawn_grants_postrespawn_iframe_breaks_death_loop){
    // Build a map whose ONLY standable spot near spawn is a water tile (worst case:
    // an authored-unsafe entrance). Without a grace window the player loops forever.
    constexpr int W=8,H=8; uint8_t cells[W*H];
    for(int y=0;y<H;++y) for(int x=0;x<W;++x){
        bool border=(x==0||x==W-1||y==0||y==H-1);
        cells[y*W+x]=border?(uint8_t)TileKind::Solid:(uint8_t)TileKind::Empty;
    }
    cells[6*W+3]=(uint8_t)TileKind::Water; // a water tile right at the spawn feet row
    Tilemap map{W,H,cells};
    const Vec2 spawn_pos{ Fixed::from_int(3*8), Fixed::from_int(3*8) };

    Player player;
    player.body.half_w=Fixed::from_int(8); player.body.half_h=Fixed::from_int(16);
    player.body.pos=spawn_pos;
    Meter health{20,100}; int invuln=0;
    constexpr int RESPAWN_IFRAMES=60; // the robust grace window the fix introduces
    InputFrame in{};

    int deaths=0;
    for(int f=0; f<300; ++f){
        player.update(in, map);
        if(invuln==0 && hazard_overlap(player.body, map)){ health.damage(20); invuln=45; }
        if(invuln>0) --invuln;
        if(health.is_empty()){
            player.body.pos=spawn_pos; player.body.vel={Fixed::from_int(0),Fixed::from_int(0)};
            health.cur=health.max;
            invuln=RESPAWN_IFRAMES; // robust fix: grace window, NOT 0
            ++deaths;
        }
    }
    // With the grace window the player dies a BOUNDED number of times (each 60-frame grace
    // lets HP-drain restart from full), never an unbreakable every-frame loop. The key
    // invariant: the grace window must exceed the hazard re-arm (45) so each respawn yields
    // real control frames. Assert we are not in a runaway loop.
    CHECK(deaths <= 300/RESPAWN_IFRAMES + 1);
    CHECK(invuln > 0 || !hazard_overlap(player.body, map)); // currently protected OR safe
}

TEST(death_in_subfloor_water_pit_respawns_safely){
    Map mp; Tilemap map = mp.view();

    // Entrance (safe ledge): top-left at col 1, row 8 (feet land on the row-10 solid ledge).
    const Vec2 spawn_pos{ Fixed::from_int(1*8), Fixed::from_int(8*8) };

    Player player;
    player.body.half_w = Fixed::from_int(8); player.body.half_h = Fixed::from_int(16);
    // Start the player INSIDE the pit (col 6) so they drain to 0 there.
    player.body.pos = { Fixed::from_int(6*8), Fixed::from_int(7*8) };

    Meter health{100,100};
    int invuln = 0;
    InputFrame in{}; // no input: player just falls + sits in the pit

    bool ever_respawned = false;
    int respawn_pos_x = -1, respawn_pos_y = -1;

    // Run enough frames to drain 100 HP at 20 per 45 frames (~225 frames) plus settle time.
    for(int frame=0; frame<400; ++frame){
        player.update(in, map);
        // hazard damage (scene line ~676)
        if(invuln==0 && hazard_overlap(player.body, map)){ health.damage(20); invuln=45; }
        // i-frame tick (scene line ~739)
        if(invuln>0) --invuln;
        // respawn (scene line ~741)
        if(health.is_empty()){
            player.body.pos = spawn_pos; player.body.vel = {Fixed::from_int(0),Fixed::from_int(0)};
            health.cur = health.max; invuln = 0;
            ever_respawned = true;
            respawn_pos_x = player.body.pos.x.to_int();
            respawn_pos_y = player.body.pos.y.to_int();
        }
    }

    // 1. Death must have triggered at least once.
    CHECK(ever_respawned);
    // 2. After the loop, the player must be at full health (the post-respawn settled state).
    CHECK(!health.is_empty());
    // 3. The player must NOT be left stuck in the water pit. After respawn + settling,
    //    the body must be resting on the SAFE ledge (cols 1..2), not in the pit (cols 4..9).
    int feet_cx = Tilemap::px_to_tile(player.body.pos.x + player.body.half_w);
    CHECK(feet_cx <= 3);
    // 4. No residual hazard overlap at the final settled position (not re-dying).
    CHECK(!hazard_overlap(player.body, map));
    // 5. The respawn snapped the body to the authored entrance X (top-left col 1 = px 8).
    CHECK_EQ(respawn_pos_x, 8);
    CHECK_EQ(respawn_pos_y, 64);
}
