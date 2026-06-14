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
TEST(spell_cycle_includes_grapple){
    World w; w.grant(Ability::Fire); w.grant(Ability::Ice); w.grant(Ability::Grapple);
    SpellState s; s.refresh(w);
    CHECK(s.owns(w, SpellId::Grapple));
    // L cycles Fire -> Ice -> Grapple -> Fire among owned
    s.selected = SpellId::Fire;
    s.cycle(w); CHECK(s.selected == SpellId::Ice);
    s.cycle(w); CHECK(s.selected == SpellId::Grapple);
    s.cycle(w); CHECK(s.selected == SpellId::Fire);
}
TEST(grapple_not_owned_without_ability){
    World w; w.grant(Ability::Fire);
    SpellState s;
    CHECK(!s.owns(w, SpellId::Grapple));
    s.selected = SpellId::Fire; s.cycle(w);   // only Fire owned -> stays Fire
    CHECK(s.selected == SpellId::Fire);
}

// --- ensure_valid: persist the player's choice across rooms/hub/scenes; only fall back to a
// default when the current selection is invalid (None or not owned). This is the key behavior
// that keeps a cycled tool across room transitions and stops a shrine pickup from resetting it. ---
TEST(ensure_valid_keeps_a_valid_selection){
    World w; w.grant(Ability::Fire); w.grant(Ability::Ice);
    SpellState s; s.selected = SpellId::Ice;   // a deliberately cycled choice
    s.ensure_valid(w);
    CHECK(s.selected == SpellId::Ice);         // LEFT untouched (Ice is owned)
}
TEST(ensure_valid_refreshes_when_none){
    World w; w.grant(Ability::Fire); w.grant(Ability::Ice);
    SpellState s;                              // selected defaults to None
    s.ensure_valid(w);
    CHECK(s.selected == SpellId::Fire);        // first owned
}
TEST(ensure_valid_refreshes_when_unowned){
    World w; w.grant(Ability::Fire);           // Ice NOT owned
    SpellState s; s.selected = SpellId::Ice;   // stale/invalid selection
    s.ensure_valid(w);
    CHECK(s.selected == SpellId::Fire);        // falls back to first owned
}
TEST(ensure_valid_preserves_cycled_choice_across_transition){
    // Simulate: own Fire+Ice, cycle to Ice, then a "room transition" calls ensure_valid.
    World w; w.grant(Ability::Fire); w.grant(Ability::Ice);
    SpellState s; s.refresh(w);                // Fire
    s.cycle(w);                                // -> Ice (player's choice)
    CHECK(s.selected == SpellId::Ice);
    s.ensure_valid(w);                         // room transition / scene entry
    CHECK(s.selected == SpellId::Ice);         // choice preserved, NOT reset to Fire
}
TEST(ensure_valid_shrine_pickup_does_not_reset){
    // Grabbing a NEW ability (shrine) calls ensure_valid: a valid cycled choice survives.
    World w; w.grant(Ability::Fire); w.grant(Ability::Ice);
    SpellState s; s.selected = SpellId::Ice;
    w.grant(Ability::Grapple);                 // shrine grants Stone/Grapple-style ability
    s.ensure_valid(w);
    CHECK(s.selected == SpellId::Ice);         // still Ice, not reset to Fire
}
