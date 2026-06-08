#include "test_framework.h"
#include "logic/pushable_block.h"
using namespace logic;
// 5x3 map: floor row 2 solid, a wall at (3,1)
static uint8_t cells[] = {0,0,0,0,0, 0,0,0,1,0, 1,1,1,1,1};
static Tilemap mk(){ return Tilemap{5,3,cells}; }
TEST(block_pushes_into_empty){ PushableBlock b{1,1}; CHECK(b.push(+1, mk())==true); CHECK_EQ(b.tx,2); }
TEST(block_blocked_by_wall){ PushableBlock b{2,1}; CHECK(b.push(+1, mk())==false); CHECK_EQ(b.tx,2); } // (3,1) is solid
TEST(block_blocked_by_edge){ PushableBlock b{0,1}; CHECK(b.push(-1, mk())==false); CHECK_EQ(b.tx,0); }
TEST(block_falls_into_gap){ PushableBlock b{1,0}; b.apply_gravity_step(mk()); CHECK_EQ(b.ty,1); // falls to row1 (row2 solid stops it)
  b.apply_gravity_step(mk()); CHECK_EQ(b.ty,1); }
