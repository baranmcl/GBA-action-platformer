#include "test_framework.h"
#include "logic/collision.h"
using namespace logic;
// 4x4 map, floor row at ty=3 solid, one-way platform at (1,2)
static uint8_t cells[] = {0,0,0,0, 0,0,0,0, 0,2,0,0, 1,1,1,1};
static Tilemap mk(){ return Tilemap{4,4,cells}; }
TEST(lands_on_floor){
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8), Fixed::from_int(16)}; b.vel={Fixed::from_int(0), Fixed::from_int(8)};
  move_and_collide(b, mk());
  CHECK(b.on_ground==true);
  CHECK(b.pos.y.to_int() <= 24-6); }
TEST(oneway_blocks_from_above){
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8), Fixed::from_int(10)}; b.vel={Fixed::from_int(0), Fixed::from_int(6)};
  move_and_collide(b, mk()); CHECK(b.on_ground==true); }
TEST(oneway_passes_from_below){ // one-way at (1,2): top y=16, cells y:16..23
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8), Fixed::from_int(20)}; b.vel={Fixed::from_int(0), Fixed::from_int(-6)};
  move_and_collide(b, mk());
  CHECK(b.on_ground==false);
  CHECK_EQ(b.vel.y.raw, Fixed::from_int(-6).raw);
  CHECK(b.pos.y.to_int() <= 14); }
TEST(oneway_lands_when_falling_fast){ // fast fall must NOT skip the one-way platform (top y=16)
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8), Fixed::from_int(6)}; b.vel={Fixed::from_int(0), Fixed::from_int(8)};
  move_and_collide(b, mk());
  CHECK(b.on_ground==true);
  CHECK_EQ(b.pos.y.to_int(), 10); } // bottom rests exactly on platform top: 16 - 6 = 10
TEST(no_tunneling_high_speed){
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8), Fixed::from_int(0)}; b.vel={Fixed::from_int(0), Fixed::from_int(40)};
  move_and_collide(b, mk()); CHECK(b.on_ground==true); }
TEST(ground_probe_grounds_resting_body){ // feet flush above floor (tile row 3), not rising
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8), Fixed::from_int(18)}; b.vel={Fixed::from_int(0), Fixed::from_int(0)};
  move_and_collide(b, mk()); CHECK(b.on_ground==true); }
TEST(ground_probe_airborne_not_grounded){ // high above floor, nothing beneath the feet
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8), Fixed::from_int(2)}; b.vel={Fixed::from_int(0), Fixed::from_int(0)};
  move_and_collide(b, mk()); CHECK(b.on_ground==false); }
TEST(aabb_overlap_basic){
  Body a{}; a.half_w=Fixed::from_int(4); a.half_h=Fixed::from_int(4); a.pos={Fixed::from_int(0),Fixed::from_int(0)};
  Body b{}; b.half_w=Fixed::from_int(4); b.half_h=Fixed::from_int(4); b.pos={Fixed::from_int(4),Fixed::from_int(4)};
  CHECK(aabb_overlap(a,b)==true);
  b.pos={Fixed::from_int(20),Fixed::from_int(0)}; CHECK(aabb_overlap(a,b)==false); }
