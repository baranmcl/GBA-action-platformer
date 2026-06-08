#include "test_framework.h"
#include "logic/wind.h"
using namespace logic;
static Body at(int x,int y){ Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(x),Fixed::from_int(y)}; return b; }
TEST(updraft_detected){
  uint8_t c[]={0,(uint8_t)TileKind::Updraft}; Tilemap m{2,1,c};
  CHECK(updraft_overlap(at(8,0),m)); CHECK(!updraft_overlap(at(0,0),m)); }
TEST(wind_direction_signed){
  uint8_t c[]={(uint8_t)TileKind::WindLeft,0,(uint8_t)TileKind::WindRight}; Tilemap m{3,1,c};
  CHECK_EQ(wind_dir(at(0,0),m), -1);   // over WindLeft -> push left
  CHECK_EQ(wind_dir(at(8,0),m),  0);   // empty
  CHECK_EQ(wind_dir(at(16,0),m), 1); } // over WindRight -> push right
