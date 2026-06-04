#include "test_framework.h"
#include "logic/meters.h"
using namespace logic;
TEST(health_damage_clamps){ Meter h{100,100}; h.damage(30); CHECK_EQ(h.cur,70); h.damage(999); CHECK_EQ(h.cur,0); CHECK(h.is_empty()); }
TEST(health_heal_clamps){ Meter h{50,100}; h.heal(70); CHECK_EQ(h.cur,100); }
TEST(magic_spend_insufficient){ Meter m{10,100}; CHECK(m.spend(20)==false); CHECK_EQ(m.cur,10); }
TEST(magic_spend_ok){ Meter m{30,100}; CHECK(m.spend(20)==true); CHECK_EQ(m.cur,10); }
TEST(magic_gain_clamps){ Meter m{90,100}; m.heal(50); CHECK_EQ(m.cur,100); }
