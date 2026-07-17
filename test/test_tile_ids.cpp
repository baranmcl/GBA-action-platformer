#include "test_framework.h"
#include "logic/tile_ids.h"
#include "logic/tilemap.h"
#include "logic/gates.h"
using namespace logic;

TEST(bg_for_kind_lava){ CHECK_EQ(tiles::bg_for_kind((int)TileKind::Lava), tiles::LAVA); CHECK_EQ(tiles::LAVA, 13); }
TEST(bg_for_kind_water){ CHECK_EQ(tiles::bg_for_kind(4), 16); }
TEST(bg_for_kind_identity_ground){ CHECK_EQ(tiles::bg_for_kind(1), 1); } // Solid/ground kinds pass through unchanged

// Cross-check: every GateType's closed-state bg_tile (gates.h GATE_TABLE) must equal
// the matching tiles:: constant — pins the two tile-index tables together so they
// can't silently drift apart.
TEST(gate_table_matches_tile_ids){
    CHECK_EQ(gate_info(GateType::Gap).bg_tile,          tiles::GATE_CLOSED);
    CHECK_EQ(gate_info(GateType::GrapplePoint).bg_tile, tiles::GATE_CLOSED);
    CHECK_EQ(gate_info(GateType::Vine).bg_tile,         tiles::VINE);
    CHECK_EQ(gate_info(GateType::Ice).bg_tile,          tiles::ICE_WALL);
    CHECK_EQ(gate_info(GateType::Water).bg_tile,        tiles::WATERFALL);
    CHECK_EQ(gate_info(GateType::CrackedWall).bg_tile,  tiles::CRACKED_WALL);
    CHECK_EQ(gate_info(GateType::CrackedFloor).bg_tile, tiles::CRACKED_FLOOR);
    CHECK_EQ(gate_info(GateType::DarkVeil).bg_tile,     tiles::DARK_VEIL);
    CHECK_EQ(gate_info(GateType::FireWall).bg_tile,     tiles::FIREWALL);
}
