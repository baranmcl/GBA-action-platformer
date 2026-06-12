#include "test_framework.h"
#include "logic/grapple.h"
#include "logic/tilemap.h"
#include "logic/physics.h"   // Body
#include "logic/fixed.h"
#include "logic/player.h"
using namespace logic;

// Shared 12x9 all-Empty map with two GrapplePoint anchors on mid-row 4:
// (8,4) [ahead of a right-facing player] and (2,4) [behind]. Big enough for the 4-tile player.
static uint8_t G_CELLS[12*9];
static Tilemap make_map(){
    for(int i=0;i<12*9;++i) G_CELLS[i]=(uint8_t)TileKind::Empty;
    G_CELLS[4*12 + 8] = (uint8_t)TileKind::GrapplePoint;   // (8,4)
    G_CELLS[4*12 + 2] = (uint8_t)TileKind::GrapplePoint;   // (2,4)
    return Tilemap{ 12, 9, G_CELLS };
}

TEST(anchor_picks_nearest_ahead){
    Tilemap m = make_map();
    int ax, ay;
    // player at tile (5,4) facing right, range 6 -> ahead anchor (8,4) chosen (behind (2,4) excluded)
    CHECK(nearest_grapple_anchor(m, 5, 4, /*facing*/1, /*range*/6, ax, ay));
    CHECK_EQ(ax, 8); CHECK_EQ(ay, 4);
}
TEST(anchor_excludes_behind){
    Tilemap m = make_map();
    int ax, ay;
    // player at (10,4) facing right: (8,4) and (2,4) are both behind -> none targetable
    CHECK(!nearest_grapple_anchor(m, 10, 4, 1, 6, ax, ay));
}
TEST(anchor_respects_range){
    Tilemap m = make_map();
    int ax, ay;
    // player at (5,4) facing right, range 1 -> (8,4) is 3 away, out of range
    CHECK(!nearest_grapple_anchor(m, 5, 4, 1, 1, ax, ay));
}
TEST(anchor_above_is_targetable){
    constexpr int W=5,H=7; uint8_t c[W*H]; for(int i=0;i<W*H;++i) c[i]=(uint8_t)TileKind::Empty;
    c[1*W + 2] = (uint8_t)TileKind::GrapplePoint;   // (2,1) directly above (2,4)
    Tilemap m{ W,H,c };
    int ax, ay;
    CHECK(nearest_grapple_anchor(m, 2, 4, /*facing*/1, 6, ax, ay));   // directly above -> targetable
    CHECK_EQ(ax,2); CHECK_EQ(ay,1);
}

// Body "at tile (tx,ty)" = top-left at (tx*8, ty*8). The 2x4-tile player's CENTRE is therefore
// at (tx*8+8, ty*8+16) = tile (tx+1, ty+2) — the latch scans from that centre tile.
static Body body_at_tile(int tx, int ty){
    Body b{}; b.half_w = Fixed::from_int(8); b.half_h = Fixed::from_int(16);
    b.pos = { Fixed::from_int(tx*8), Fixed::from_int(ty*8) };
    return b;
}

TEST(grapple_latches_only_with_ability_and_anchor){
    Tilemap m = make_map();                 // anchor at (8,4)
    Body b = body_at_tile(5,4);             // centre tile (6,6); anchor (8,4) is ahead + up, in range
    GrappleState g;
    CHECK(!g.latch(true, b, /*facing*/1, m, /*has*/false)); // no ability
    CHECK(!g.active());
    CHECK(g.latch(true, b, 1, m, /*has*/true));             // ability + anchor in arc -> latched
    CHECK(g.active());
}
TEST(grapple_no_latch_without_fire){
    Tilemap m = make_map(); Body b = body_at_tile(5,4); GrappleState g;
    CHECK(!g.latch(false, b, 1, m, true));   // not fired
    CHECK(!g.active());
}
TEST(grapple_pull_velocity_points_at_anchor){
    Tilemap m = make_map(); Body b = body_at_tile(5,4); GrappleState g;
    g.latch(true, b, 1, m, true);            // anchor (8,4) -> centre px (68,36); player centre (48,48)
    Vec2 v = g.pull_velocity(b);
    CHECK(v.x.raw > 0);                       // pulling right (anchor is right of player)
    CHECK(v.x <= GrappleState::GRAPPLE_VX);   // per-axis magnitude clamped to GRAPPLE_VX
    CHECK(v.y.raw < 0);                       // anchor is above the player -> pulled up
}
TEST(grapple_continues_mid_pull){
    Tilemap m = make_map(); GrappleState g;
    Body b = body_at_tile(5,4);          // centre tile (6,6); anchor (8,4) — not yet reached
    g.latch(true, b, 1, m, true);
    g.post(b, m, /*moved*/true);         // moved, but centre tile != anchor tile
    CHECK(g.active());                   // still pulling (did not terminate early)
}
TEST(grapple_ends_on_arrival){
    Tilemap m = make_map(); GrappleState g;
    Body b = body_at_tile(5,4);
    g.latch(true, b, 1, m, true);             // target = anchor tile (8,4)
    b.pos = { Fixed::from_int(60), Fixed::from_int(20) };  // body centre = (68,36) -> centre tile (8,4)
    g.post(b, m, /*moved*/true);
    CHECK(!g.active());                        // reached the anchor tile -> ended
}
TEST(grapple_ends_when_blocked){
    Tilemap m = make_map(); GrappleState g;
    Body b = body_at_tile(5,4);
    g.latch(true, b, 1, m, true);
    g.post(b, m, /*moved*/false);   // collision stopped the body before arrival
    CHECK(!g.active());
}

TEST(grapple_arrival_snaps_onto_platform_and_zeros_velocity){
    // 12x9 map, anchor at (8,4), a solid platform directly below it at (8,5).
    uint8_t c[12*9]; for(int i=0;i<12*9;++i) c[i]=(uint8_t)TileKind::Empty;
    c[4*12 + 8] = (uint8_t)TileKind::GrapplePoint;   // anchor (8,4)
    c[5*12 + 8] = (uint8_t)TileKind::Solid;          // platform (8,5)
    Tilemap m{ 12, 9, c };
    GrappleState g;
    Body b = body_at_tile(5,4);
    g.latch(true, b, 1, m, true);                    // anchor (8,4) ahead+up
    b.pos = { Fixed::from_int(60), Fixed::from_int(20) };  // centre (68,36) -> centre tile (8,4): arrived
    b.vel = { Fixed::from_int(5), Fixed::from_int(5) };    // residual pull velocity
    g.post(b, m, true);
    CHECK(!g.active());
    CHECK_EQ(b.vel.x.raw, 0); CHECK_EQ(b.vel.y.raw, 0);    // velocity zeroed (no overshoot)
    // feet on top of the platform (8,5): feet y = 5*8 = 40 -> pos.y = 40 - 2*16 = 8
    CHECK_EQ(b.pos.y.raw, Fixed::from_int(5*8 - 32).raw);
    // centred on anchor column 8: pos.x = 8*8+4-8 = 60
    CHECK_EQ(b.pos.x.raw, Fixed::from_int(8*8 + 4 - 8).raw);
}
TEST(grapple_works_mid_air){
    // anchor (8,4); no floor under the player's start -> player is airborne when firing.
    Tilemap m = make_map();                          // all-empty 12x9 (OOB solid), anchor (8,4)
    Player p; p.body.half_w = Fixed::from_int(8); p.body.half_h = Fixed::from_int(16);
    p.body.pos = { Fixed::from_int(4*8), Fixed::from_int(3*8) };
    p.facing = 1; p.abilities.grapple = true;
    p.body.on_ground = false;                        // airborne
    InputFrame fire{}; fire.grapple_fire = true;
    Fixed x0 = p.body.pos.x;
    p.update(fire, m);                               // should latch + pull even though airborne
    CHECK(p.grapple.active() || p.body.pos.x > x0);  // latched and/or moved toward the anchor
    CHECK(p.body.pos.x > x0);                         // pulled right toward (8,4) while airborne
}

TEST(player_update_pulls_toward_anchor){
    // 14x12 map: solid border + a floor at row 9; one anchor to the right at the player's height.
    uint8_t cells[14*12];
    for(int i=0;i<14*12;++i) cells[i]=(uint8_t)TileKind::Empty;
    for(int x=0;x<14;++x){ cells[0*14+x]=(uint8_t)TileKind::Solid; cells[11*14+x]=(uint8_t)TileKind::Solid; }
    for(int y=0;y<12;++y){ cells[y*14+0]=(uint8_t)TileKind::Solid; cells[y*14+13]=(uint8_t)TileKind::Solid; }
    for(int x=0;x<14;++x) cells[9*14+x]=(uint8_t)TileKind::Solid;          // floor row 9
    cells[7*14 + 9] = (uint8_t)TileKind::GrapplePoint;                     // anchor (9,7), right of player
    Tilemap m{ 14, 12, cells };

    Player p;
    p.body.half_w = Fixed::from_int(8); p.body.half_h = Fixed::from_int(16);
    p.body.pos = { Fixed::from_int(3*8), Fixed::from_int(5*8) };           // drop onto the floor
    p.facing = 1; p.abilities.grapple = true;

    InputFrame idle{};
    for(int i=0;i<20;++i) p.update(idle, m);                              // settle on the floor
    Fixed x0 = p.body.pos.x;
    InputFrame fire{}; fire.grapple_fire = true;
    p.update(fire, m);                                                    // latch + first pull step
    CHECK(p.body.pos.x > x0);                                             // moved right toward (9,7)
    InputFrame hold{};
    for(int i=0;i<40 && p.grapple.active(); ++i) p.update(hold, m);       // generous cap; loop exits early via active() guard once the pull ends
    CHECK(!p.grapple.active());                                           // ended (centre tile reached anchor, or wall)
    CHECK(p.body.pos.x > x0);
    CHECK_EQ(p.body.vel.x.raw, 0);                                        // velocity zeroed on arrival (no overshoot)
    // feet at floor row 9: feet_y = 9*8 = 72 -> pos.y = 72 - 2*16 = 40
    CHECK_EQ(p.body.pos.y.raw, Fixed::from_int(9*8 - 32).raw);
}
