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
TEST(gate_cleared_by_spell){
  CHECK(gate_cleared_by(GateType::Vine)  == SpellId::Fire);
  CHECK(gate_cleared_by(GateType::Ice)   == SpellId::Fire);
  CHECK(gate_cleared_by(GateType::Water)    == SpellId::Ice);
  CHECK(gate_cleared_by(GateType::FireWall) == SpellId::Ice);   // Ice extinguishes a fire wall
  CHECK(gate_cleared_by(GateType::Gap)      == SpellId::None); } // geometry gate, not spell-cleared

// --- M10 Light ---
TEST(spell_for_ability_light){
    CHECK(spell_for_ability(Ability::Light) == SpellId::Light);
}
TEST(dark_veil_cleared_by_light){
    CHECK(gate_cleared_by(GateType::DarkVeil) == SpellId::Light);
}
TEST(dark_veil_needs_light_ability){
    CHECK(!can_pass(GateType::DarkVeil, 0));
    uint16_t light_bit = (uint16_t)(1u << (int)Ability::Light);
    CHECK(can_pass(GateType::DarkVeil, light_bit));
}
