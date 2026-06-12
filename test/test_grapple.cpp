#include "test_framework.h"
#include "logic/grapple.h"
#include "logic/tilemap.h"
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
    uint8_t c[5*7]; for(int i=0;i<35;++i) c[i]=(uint8_t)TileKind::Empty;
    c[1*5 + 2] = (uint8_t)TileKind::GrapplePoint;   // (2,1) directly above (2,4)
    Tilemap m{ 5,7,c };
    int ax, ay;
    CHECK(nearest_grapple_anchor(m, 2, 4, /*facing*/1, 6, ax, ay));   // directly above -> targetable
    CHECK_EQ(ax,2); CHECK_EQ(ay,1);
}
