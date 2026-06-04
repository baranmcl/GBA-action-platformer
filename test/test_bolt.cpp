#include "test_framework.h"
#include "logic/bolt.h"
using namespace logic;
TEST(bolt_fires_then_cooldown){ BoltCaster c; c.cooldown_ticks=10;
  BoltSpawn out; CHECK(c.try_fire(true, Vec2{Fixed::from_int(5),Fixed::from_int(5)}, +1, out)==true);
  CHECK(out.vel.x.raw>0);
  CHECK(c.try_fire(true, Vec2{}, +1, out)==false); // still cooling
  for(int i=0;i<10;++i) c.tick();
  CHECK(c.try_fire(true, Vec2{}, +1, out)==true); }
TEST(bolt_respects_facing){ BoltCaster c; c.cooldown_ticks=0; BoltSpawn out;
  c.try_fire(true, Vec2{}, -1, out); CHECK(out.vel.x.raw<0); }
TEST(bolt_no_fire_without_input){ BoltCaster c; BoltSpawn out; CHECK(c.try_fire(false, Vec2{}, 1, out)==false); }
