#include "test_framework.h"
#include "logic/tilemap.h"
using namespace logic;
static uint8_t cells[] = { 0,0,1, 2,2,2, 0,0,0 };
static Tilemap mk(){ return Tilemap{3,3,cells}; }
TEST(tile_lookup_solid){ CHECK(mk().at(2,0)==TileKind::Solid); }
TEST(tile_lookup_oneway){ CHECK(mk().at(0,1)==TileKind::OneWay); }
TEST(tile_out_of_bounds_is_solid){ CHECK(mk().at(-1,0)==TileKind::Solid); CHECK(mk().at(3,0)==TileKind::Solid); }
TEST(tile_solid_query){ CHECK(mk().is_solid(2,0)==true); CHECK(mk().is_solid(0,0)==false); }
TEST(px_to_tile){ CHECK_EQ(Tilemap::px_to_tile(Fixed::from_int(15)), 1); CHECK_EQ(Tilemap::px_to_tile(Fixed::from_int(8)),1); CHECK_EQ(Tilemap::px_to_tile(Fixed::from_int(7)),0); }
TEST(tile_lava_is_not_solid_but_is_lava){
  uint8_t lcells[] = {0,3,1};  // 3 = lava
  Tilemap m{3,1,lcells};
  CHECK(m.is_lava(1,0)); CHECK(!m.is_lava(0,0));
  CHECK(!m.is_solid(1,0));      // lava is NOT solid (you fall in)
  CHECK(!m.is_oneway(1,0)); }
TEST(water_and_ice_kinds){
  uint8_t cells4[] = { (uint8_t)TileKind::Empty, (uint8_t)TileKind::Solid,
                       (uint8_t)TileKind::Water, (uint8_t)TileKind::IcePlatform };
  Tilemap m{4,1,cells4};
  CHECK(m.is_water(2,0));
  CHECK(!m.is_solid(2,0));      // water is NOT solid
  CHECK(m.is_solid(3,0));       // ice platform IS solid (stand on it)
  CHECK(m.is_solid(1,0));       // regular solid still solid
  CHECK(!m.is_water(3,0)); }
