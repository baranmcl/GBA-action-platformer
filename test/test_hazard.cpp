#include "test_framework.h"
#include "logic/hazard.h"
using namespace logic;
TEST(body_over_lava_detected){
  uint8_t cells[] = {0,0, 3,3};      // bottom row lava (row1)
  Tilemap m{2,2,cells};
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(2), Fixed::from_int(8)};   // overlaps row1 (y8..)
  CHECK(lava_overlap(b,m)==true);
  b.pos={Fixed::from_int(2), Fixed::from_int(0)};   // row0 only
  CHECK(lava_overlap(b,m)==false); }
