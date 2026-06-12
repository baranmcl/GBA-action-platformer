#include "test_framework.h"
#include "logic/pushable_block.h"
#include "logic/level_data.h"
using namespace logic;
// 5x3 map: floor row 2 solid, a wall at (3,1)
static uint8_t cells[] = {0,0,0,0,0, 0,0,0,1,0, 1,1,1,1,1};
static Tilemap mk(){ return Tilemap{5,3,cells}; }
TEST(block_pushes_into_empty){ PushableBlock b{1,1}; CHECK(b.push(+1, mk())==true); CHECK_EQ(b.tx,2); }
TEST(block_blocked_by_wall){ PushableBlock b{2,1}; CHECK(b.push(+1, mk())==false); CHECK_EQ(b.tx,2); } // (3,1) is solid
TEST(block_blocked_by_edge){ PushableBlock b{0,1}; CHECK(b.push(-1, mk())==false); CHECK_EQ(b.tx,0); }
TEST(block_falls_into_gap){ PushableBlock b{1,0}; b.apply_gravity_step(mk()); CHECK_EQ(b.ty,1); // falls to row1 (row2 solid stops it)
  b.apply_gravity_step(mk()); CHECK_EQ(b.ty,1); }
TEST(block_pull_slides_one_tile_toward_dir){
    uint8_t pull_cells[5*3]; for(int i=0;i<15;++i) pull_cells[i]=(uint8_t)logic::TileKind::Empty;
    logic::Tilemap m{ 5,3, pull_cells };
    logic::PushableBlock blk{ 3, 1 };
    CHECK(blk.pull(-1, m));     // pull left toward the player
    CHECK_EQ(blk.tx, 2);
    CHECK(blk.pull(-1, m));
    CHECK_EQ(blk.tx, 1);
}
TEST(block_pull_blocked_by_wall){
    uint8_t wall_cells[3*3]; for(int i=0;i<9;++i) wall_cells[i]=(uint8_t)logic::TileKind::Empty;
    wall_cells[1*3 + 0] = (uint8_t)logic::TileKind::Solid;   // wall at (0,1)
    logic::Tilemap m{ 3,3, wall_cells };
    logic::PushableBlock blk{ 1, 1 };
    CHECK(!blk.pull(-1, m));    // wall to the left -> no move
    CHECK_EQ(blk.tx, 1);
}
TEST(blockspawn_pullable_defaults_false){
    logic::BlockSpawn b{ 4, 5 };       // old 2-field literal still compiles
    CHECK(!b.pullable);
    logic::BlockSpawn b2{ 4, 5, true };
    CHECK(b2.pullable);
}
