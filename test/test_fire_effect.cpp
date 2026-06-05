#include "test_framework.h"
#include "logic/fire_effect.h"
#include "logic/gates.h"
using namespace logic;
TEST(fire_clears_vine_and_ice){ CHECK(fire_clears_gate(GateType::Vine)); CHECK(fire_clears_gate(GateType::Ice)); }
TEST(fire_does_not_clear_gap){ CHECK(!fire_clears_gate(GateType::Gap)); } // gap needs Featherleap, not Fire
