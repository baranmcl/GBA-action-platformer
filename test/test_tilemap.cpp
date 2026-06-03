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
