#include "test_framework.h"
#include "logic/puzzle.h"
using namespace logic;
TEST(brazier_group_active_when_all_lit){ Trigger t = Trigger::braziers(3);
  t.lit=2; CHECK(!t.active()); t.lit=3; CHECK(t.active()); t.lit=4; CHECK(t.active()); }
TEST(plate_active_when_pressed){ Trigger t = Trigger::plate(); CHECK(!t.active()); t.pressed=true; CHECK(t.active()); }
TEST(button_active_when_pressed){ Trigger t = Trigger::button(); CHECK(!t.active()); t.pressed=true; CHECK(t.active()); }
