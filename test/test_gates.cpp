#include "test_framework.h"
#include "logic/gates.h"
#include "logic/world_state.h"
using namespace logic;
static uint16_t bit(Ability a){ return (uint16_t)(1u << (int)a); }
TEST(gap_needs_featherleap){ CHECK(!can_pass(GateType::Gap, 0)); CHECK(can_pass(GateType::Gap, bit(Ability::Featherleap))); }
TEST(ice_needs_fire){ CHECK(!can_pass(GateType::Ice, 0)); CHECK(can_pass(GateType::Ice, bit(Ability::Fire))); CHECK(!can_pass(GateType::Ice, bit(Ability::Ice))); }
TEST(water_needs_ice){ CHECK(can_pass(GateType::Water, bit(Ability::Ice))); CHECK(!can_pass(GateType::Water, bit(Ability::Fire))); }
TEST(gate_geometry_flags){ CHECK(gate_info(GateType::Gap).is_geometry); CHECK(!gate_info(GateType::Ice).is_geometry); }
TEST(gap_closed_visual_is_gate_tile){ CHECK_EQ((int)gate_info(GateType::Gap).bg_tile, 3); }
