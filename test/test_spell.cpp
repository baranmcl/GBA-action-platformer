#include "test_framework.h"
#include "logic/spell.h"
#include "logic/meters.h"
#include "logic/world_state.h"
using namespace logic;
TEST(fire_needs_magic){ SpellCast fc; Meter magic{5,100}; BoltSpawn out;
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==false); CHECK_EQ(magic.cur,5); }     // insufficient (cost 10)
TEST(fire_spends_and_spawns){ SpellCast fc; Meter magic{50,100}; BoltSpawn out;
  CHECK(fc.try_cast(true, magic, Vec2{Fixed::from_int(5),Fixed::from_int(5)}, 1, out)==true);
  CHECK_EQ(magic.cur,40); CHECK(out.vel.x.raw>0); }                                    // spent 10, facing +
TEST(fire_cooldown){ SpellCast fc; Meter magic{100,100}; BoltSpawn out;
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==true);
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==false);   // cooling
  for(int i=0;i<fc.cooldown_ticks;++i) fc.tick();
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==true); }
TEST(spellstate_availability){ SpellState s; World w;
  CHECK(!s.has_any(w));                       // no spells before any ability
  w.grant(Ability::Fire); s.refresh(w);
  CHECK(s.has_any(w)); CHECK(s.selected==SpellId::Fire); }
TEST(cycle_rotates_owned_spells){ SpellState s; World w;
  w.grant(Ability::Fire); s.refresh(w);
  CHECK(s.selected==SpellId::Fire);
  s.cycle(w); CHECK(s.selected==SpellId::Fire);   // only one owned -> stays
  w.grant(Ability::Ice); s.cycle(w);              // now two owned
  CHECK(s.selected==SpellId::Ice);
  s.cycle(w); CHECK(s.selected==SpellId::Fire); } // wraps
TEST(refresh_prefers_an_owned_spell){ SpellState s; World w;
  w.grant(Ability::Ice); s.refresh(w);
  CHECK(s.selected==SpellId::Ice); }              // owns Ice but not Fire
