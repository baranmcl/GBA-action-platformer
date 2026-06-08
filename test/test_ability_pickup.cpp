#include "test_framework.h"
#include "logic/ability_pickup.h"
using namespace logic;
static Body mk(int x,int y){ Body b{}; b.half_w=Fixed::from_int(6); b.half_h=Fixed::from_int(6);
  b.pos={Fixed::from_int(x),Fixed::from_int(y)}; return b; }
TEST(pickup_grants_on_overlap){ World w; Body p=mk(8,8), shrine=mk(10,10);
  CHECK(try_take_pickup(p, shrine, w, Ability::Fire)==true); CHECK(w.has(Ability::Fire)); }
TEST(pickup_idempotent){ World w; w.grant(Ability::Fire); Body p=mk(8,8), shrine=mk(10,10);
  CHECK(try_take_pickup(p, shrine, w, Ability::Fire)==false); CHECK(w.has(Ability::Fire)); } // already had it
TEST(pickup_no_overlap){ World w; Body p=mk(0,0), shrine=mk(99,0);
  CHECK(try_take_pickup(p, shrine, w, Ability::Fire)==false); CHECK(!w.has(Ability::Fire)); }
