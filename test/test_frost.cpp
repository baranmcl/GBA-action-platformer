#include "test_framework.h"
#include "logic/frost.h"
using namespace logic;
TEST(ice_freezes_only_water){
  CHECK(ice_freezes(TileKind::Water)==TileKind::IcePlatform);
  CHECK(ice_freezes(TileKind::Empty)==TileKind::Empty);   // unchanged
  CHECK(ice_freezes(TileKind::Solid)==TileKind::Solid);   // never affects walls
}
TEST(fire_melts_only_ice_platform){
  CHECK(fire_melts(TileKind::IcePlatform)==TileKind::Water);
  CHECK(fire_melts(TileKind::Solid)==TileKind::Solid);    // never melts walls
  CHECK(fire_melts(TileKind::Water)==TileKind::Water);    // unchanged
}
