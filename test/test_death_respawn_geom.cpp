#include "test_framework.h"
#include "logic/player.h"
#include "logic/meters.h"
#include "logic/hazard.h"
#include "logic/collision.h"
using namespace logic;

// Repro of the REAL d7_room2 water-tunnel geometry to check whether a player who falls
// into the sub-floor water pit actually OVERLAPS the water (and thus drains to 0). If the
// body rests on the solid border BELOW the single water row and its AABB no longer touches
// the water, hazard_overlap stays false forever -> HP never drains -> no death -> the
// player is STUCK at the bottom with no respawn. That is the QA "stuck, no respawn" report.
namespace {
    // d7_room2 relevant slice (40 wide). Water is a SINGLE row (grid row 20), border below (row 21).
    // We model a 12x12 cutout with the same vertical relationship:
    //   row 9  : open (normal floor level is absent here -> the pit mouth)
    //   row 10 : SINGLE water row (the sub-floor hazard)
    //   row 11 : solid border (bottom)
    struct Map {
        static constexpr int W=12, H=12;
        uint8_t cells[W*H];
        Map(){
            for(int y=0;y<H;++y) for(int x=0;x<W;++x){
                bool border=(x==0||x==W-1||y==H-1);
                cells[y*W+x]=border?(uint8_t)TileKind::Solid:(uint8_t)TileKind::Empty;
            }
            for(int x=4;x<=9;++x) cells[10*W+x]=(uint8_t)TileKind::Water; // single water row above border
        }
        Tilemap view() const { return Tilemap{W,H,cells}; }
    };
}

TEST(player_falling_into_single_water_row_pit_still_overlaps_and_drains){
    Map mp; Tilemap map=mp.view();
    Player player;
    player.body.half_w=Fixed::from_int(8); player.body.half_h=Fixed::from_int(16);
    player.body.pos={Fixed::from_int(6*8), Fixed::from_int(5*8)}; // drop in above the pit
    InputFrame in{};
    // Let the body settle.
    for(int f=0; f<60; ++f) player.update(in, map);
    // Player is 4 tiles tall; rests with feet on the row-11 border, body spans rows ~7..10,
    // so it DOES overlap the water row 10. Confirm the hazard fires.
    bool haz = hazard_overlap(player.body, map);
    int feet_row = Tilemap::px_to_tile(player.body.pos.y + player.body.half_h + player.body.half_h - Fixed::from_int(1));
    std::printf("  [geom] settled top-left y=%d feet_row=%d hazard=%d\n",
                player.body.pos.y.to_int(), feet_row, (int)haz);
    CHECK(haz); // if this FAILS, the player can sit in a 1-row pit without ever taking damage
}
