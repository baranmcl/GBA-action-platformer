#include "test_framework.h"
#include "logic/boss.h"
using namespace logic;

// Land exactly one wound the way the fight now requires: run out any post-wound i-frames, expose with
// Light, then wound. (A wound ends the expose + grants i-frames, so wounds must be paced like this.)
static void wound_once(BossState& b){
    for(int i = 0; i < KING_DEF.hit_iframes + 1; ++i) b.tick();   // clear post-wound i-frames
    b.on_expose_hit(SpellId::Light);                              // expose
    b.on_wound(KING_DEF.wound_dmg);                              // one wound (ends expose, sets i-frames)
}

TEST(boss_initial_state){
    BossState b;
    b.reset(KING_DEF); // sets P1 + full HP + cleared timers
    CHECK_EQ(b.phase, 0);
    CHECK_EQ(b.hp, KING_DEF.max_hp);
    CHECK_EQ(b.phase_start_hp, KING_DEF.max_hp);
    CHECK_EQ(b.expose_timer, 0);
    CHECK(!b.exposed());
    CHECK(!b.defeated());
}

TEST(boss_constants_sane){
    CHECK_EQ(KING_DEF.max_hp, 90);
    CHECK(KING_PHASES[0].end_hp > KING_PHASES[1].end_hp);
    CHECK(KING_PHASES[1].end_hp > 0);
    CHECK_EQ(KING_DEF.max_hp % KING_DEF.wound_dmg, 0); // HP is a whole number of wounds
}

TEST(boss_light_hit_exposes){
    BossState b; b.reset(KING_DEF);
    b.on_expose_hit(SpellId::Light);
    CHECK(b.exposed());
    CHECK_EQ(b.phase, 0); // phase index unchanged (combat phase held across expose)
    CHECK_EQ(b.expose_timer, KING_DEF.expose_frames);
}

TEST(boss_light_hit_while_already_exposed_refreshes){
    BossState b; b.reset(KING_DEF);
    b.on_expose_hit(SpellId::Light);
    b.tick(); // expose_timer 75 -> 74
    b.on_expose_hit(SpellId::Light); // refresh; phase index unchanged
    CHECK_EQ(b.expose_timer, KING_DEF.expose_frames);
    CHECK_EQ(b.phase, 0);
}

TEST(boss_expose_decays_to_exact_zero_then_returns){
    BossState b; b.reset(KING_DEF);
    b.on_expose_hit(SpellId::Light);        // expose_timer = 75, phase index 0
    for(int i = 0; i < KING_DEF.expose_frames - 1; ++i) b.tick();
    CHECK(b.exposed());                      // still >0 at frame 74
    CHECK_EQ(b.expose_timer, 1);
    CHECK_EQ(b.phase, 0);
    b.tick();                                // 1 -> 0
    CHECK(!b.exposed());
    CHECK_EQ(b.phase, 0); // phase index unchanged across expose
}

TEST(boss_attack_timer_advances_and_wraps_per_phase){
    BossState b; b.reset(KING_DEF);          // phase 0 (P1)
    int period = KING_PHASES[0].pattern.telegraph_frames
               + KING_PHASES[0].pattern.attack_active_frames
               + KING_PHASES[0].pattern.recovery_frames;
    for(int i = 0; i < period; ++i) b.tick();
    CHECK_EQ(b.attack_timer, 0);             // wrapped exactly at the phase period
}

TEST(boss_attack_frozen_while_exposed){
    // The expose window is a CLEAN damage window — the King's attack pattern must NOT
    // advance while EXPOSED (so the player can safely switch spells + wound; co-design rule).
    BossState b; b.reset(KING_DEF);
    for(int i = 0; i < 5; ++i) b.tick();     // advance the attack pattern a little
    int frozen = b.attack_timer;
    b.on_expose_hit(SpellId::Light);         // EXPOSED
    for(int i = 0; i < 10; ++i) b.tick();    // ticks during expose
    CHECK_EQ(b.attack_timer, frozen);        // unchanged while exposed
}

TEST(boss_wound_only_while_exposed){
    BossState b; b.reset(KING_DEF);
    b.on_wound(KING_DEF.wound_dmg);          // NOT exposed -> no effect (shielded)
    CHECK_EQ(b.hp, KING_DEF.max_hp);
    b.on_expose_hit(SpellId::Light);
    b.on_wound(KING_DEF.wound_dmg);          // exposed -> subtract
    CHECK_EQ(b.hp, KING_DEF.max_hp - KING_DEF.wound_dmg);
}

TEST(boss_wound_ends_expose_and_sets_iframes){
    BossState b; b.reset(KING_DEF);
    b.on_expose_hit(SpellId::Light);
    CHECK(b.exposed());
    b.on_wound(KING_DEF.wound_dmg);
    CHECK(!b.exposed());                       // a wound ends the expose window
    CHECK_EQ(b.hit_iframes, KING_DEF.hit_iframes);
    CHECK_EQ(b.hp, KING_DEF.max_hp - KING_DEF.wound_dmg);
}

TEST(boss_one_wound_per_expose){
    BossState b; b.reset(KING_DEF);
    b.on_expose_hit(SpellId::Light);
    b.on_wound(KING_DEF.wound_dmg);                     // lands
    b.on_wound(KING_DEF.wound_dmg);                     // blocked (expose ended + i-frames)
    CHECK_EQ(b.hp, KING_DEF.max_hp - KING_DEF.wound_dmg);
}

TEST(boss_cannot_reexpose_during_iframes){
    BossState b; b.reset(KING_DEF);
    b.on_expose_hit(SpellId::Light); b.on_wound(KING_DEF.wound_dmg);   // now in i-frames
    b.on_expose_hit(SpellId::Light);                          // blocked while i-frames > 0
    CHECK(!b.exposed());
    for(int i = 0; i < KING_DEF.hit_iframes; ++i) b.tick();   // i-frames decay
    CHECK_EQ(b.hit_iframes, 0);
    b.on_expose_hit(SpellId::Light);                          // works again
    CHECK(b.exposed());
}

TEST(boss_wound_crossing_threshold_advances_phase){
    BossState b; b.reset(KING_DEF);
    while(b.hp > KING_PHASES[0].end_hp) wound_once(b);  // 90 -> 60 (crosses), one wound per Light
    CHECK_EQ(b.hp, KING_PHASES[0].end_hp);
    CHECK_EQ(b.phase, 1); // underlying phase advanced to index 1 (P2)
    CHECK_EQ(b.phase_start_hp, KING_PHASES[0].end_hp);
}

TEST(boss_wound_to_zero_defeats){
    BossState b; b.reset(KING_DEF);
    int guard = 0;
    while(!b.defeated() && guard++ < 100) wound_once(b);
    CHECK(b.defeated());
    CHECK_EQ(guard, KING_DEF.max_hp / KING_DEF.wound_dmg);  // exactly 9 wounds (one per Light) to kill
}

TEST(boss_player_death_restarts_full_fight){
    BossState b; b.reset(KING_DEF);
    while(b.hp > KING_PHASES[1].end_hp) wound_once(b);  // deep into the fight (P3)
    b.on_player_death();
    CHECK_EQ(b.phase, 0);
    CHECK_EQ(b.hp, KING_DEF.max_hp);
    CHECK_EQ(b.phase_start_hp, KING_DEF.max_hp);
    CHECK_EQ(b.expose_timer, 0);
    CHECK_EQ(b.attack_timer, 0);
    CHECK_EQ(b.hit_iframes, 0);
    CHECK(!b.exposed());
}

TEST(switch_budget_invariant_all_windows){
    // No telegraph window and no expose window may be tighter than the worst-case
    // time to cycle (L) to the right spell + react + cast. If this fails, a window
    // was set below SWITCH_BUDGET -> the phase would be unfair regardless of skill.
    for(int i = 0; i < KING_DEF.phase_count; ++i)
        CHECK(KING_PHASES[i].pattern.telegraph_frames >= SWITCH_BUDGET);
    CHECK(KING_DEF.expose_frames >= SWITCH_BUDGET);
}

TEST(phase_pattern_escalates_but_respects_budget){
    // Telegraph shrinks each phase (escalation) but never below the floor.
    CHECK(KING_PHASES[0].pattern.telegraph_frames >= KING_PHASES[1].pattern.telegraph_frames);
    CHECK(KING_PHASES[1].pattern.telegraph_frames >= KING_PHASES[2].pattern.telegraph_frames);
    CHECK(KING_PHASES[2].pattern.telegraph_frames >= SWITCH_BUDGET);
}

TEST(boss_phase_step_is_deterministic){
    // Same phase + same attack_timer -> same pattern step (no hidden state/RNG).
    BossState a; a.reset(KING_DEF); BossState b; b.reset(KING_DEF);
    for(int i=0;i<40;++i){ a.tick(); b.tick(); }
    CHECK_EQ(a.attack_timer, b.attack_timer);
    CHECK_EQ((int)a.current_step(), (int)b.current_step());
}

// ---------------------------------------------------------------------------
// M12 P1.1 — data model sanity (def-driven BossState; phase as an index).
// ---------------------------------------------------------------------------
TEST(bossdef_king_reset_initial_state){
    BossState b; b.reset(KING_DEF);
    CHECK_EQ(b.hp, KING_DEF.max_hp);          // 90
    CHECK_EQ(b.phase, 0);
    CHECK_EQ(b.phase_start_hp, KING_DEF.max_hp);
    CHECK_EQ(b.expose_timer, 0);
    CHECK_EQ(b.hit_iframes, 0);
    CHECK(!b.exposed()); CHECK(!b.defeated());
}
TEST(bossdef_king_has_three_phases){ CHECK_EQ(KING_DEF.phase_count, 3); }
TEST(bossdef_d1_has_two_phases_tired_window){
    CHECK_EQ(D1_DEF.phase_count, 2);
    CHECK((int)D1_DEF.vuln == (int)VulnMode::TiredWindow);
    CHECK_EQ(D1_DEF.tired_after, 3);
    CHECK(D1_DEF.expose_frames >= SWITCH_BUDGET);   // the tired window doubles as the safe window
}

// ---------------------------------------------------------------------------
// M12 QA r1 — D1_DEF TiredWindow behaviour. The Guardian is INVULNERABLE while
// attacking and tires (a vulnerable window) after `tired_after` completed cycles.
// ---------------------------------------------------------------------------

// Advance one full attack cycle (period frames). Returns nothing; the boss may tire
// on the wrapping tick (the last frame of the cycle).
static void run_one_cycle(BossState& b){
    int period = b.phase_period();
    for(int i = 0; i < period; ++i) b.tick();
}

TEST(d1_not_vulnerable_initially){
    BossState b; b.reset(D1_DEF);
    CHECK(!b.vulnerable());                       // TiredWindow: invulnerable while attacking
    CHECK(!b.exposed());
    b.on_wound(D1_DEF.wound_dmg);                 // a wound while not tired is shielded -> no effect
    CHECK_EQ(b.hp, D1_DEF.max_hp);
}
TEST(d1_tires_after_tired_after_cycles){
    BossState b; b.reset(D1_DEF);
    for(int c = 0; c < D1_DEF.tired_after - 1; ++c){ run_one_cycle(b); CHECK(!b.exposed()); }
    run_one_cycle(b);                             // the tired_after-th cycle opens the window
    CHECK(b.exposed());                           // TIRED -> exposed()
    CHECK(b.vulnerable());                        // exposed() => vulnerable()
    CHECK_EQ(b.expose_timer, D1_DEF.expose_frames);
    CHECK_EQ(b.attack_cycles, 0);                 // reset on tiring
}
TEST(d1_wound_during_tired_window_lands_and_ends_it){
    BossState b; b.reset(D1_DEF);
    for(int c = 0; c < D1_DEF.tired_after; ++c) run_one_cycle(b);
    CHECK(b.exposed());
    b.on_wound(D1_DEF.wound_dmg);                 // lands during the tired window
    CHECK_EQ(b.hp, D1_DEF.max_hp - D1_DEF.wound_dmg);
    CHECK(!b.exposed());                          // a wound ends the tired window
    CHECK_EQ(b.hit_iframes, D1_DEF.hit_iframes);  // and grants i-frames (one wound per window)
}
TEST(d1_one_wound_per_tired_window){
    BossState b; b.reset(D1_DEF);
    for(int c = 0; c < D1_DEF.tired_after; ++c) run_one_cycle(b);
    b.on_wound(D1_DEF.wound_dmg);                 // lands
    b.on_wound(D1_DEF.wound_dmg);                 // blocked (window ended + i-frames)
    CHECK_EQ(b.hp, D1_DEF.max_hp - D1_DEF.wound_dmg);
}
TEST(d1_attack_frozen_while_tired){
    BossState b; b.reset(D1_DEF);
    for(int c = 0; c < D1_DEF.tired_after; ++c) run_one_cycle(b);
    CHECK(b.exposed());
    int frozen = b.attack_timer;
    for(int i = 0; i < 10; ++i) b.tick();        // ticks during the tired window
    CHECK_EQ(b.attack_timer, frozen);            // attack pattern frozen while tired
}
TEST(d1_light_is_noop_for_tired_window){
    BossState b; b.reset(D1_DEF);
    b.on_expose_hit(SpellId::Light);             // TiredWindow ignores Light (no expose mechanic)
    CHECK_EQ(b.expose_timer, 0);
    CHECK(!b.exposed());
}

// ---------------------------------------------------------------------------
// M12 QA r1 — AlwaysVulnerable still works (no shipped boss uses it now, so a
// small synthetic test-only def). Guards the mode against accidental removal.
// ---------------------------------------------------------------------------
static constexpr BossPhaseDef AV_PHASES[1] = { { 0, { 60, 30, 30 }, BOSS_ATK_AIMED } };
static constexpr BossDef AV_DEF{
    20, 10, 30, 0, VulnMode::AlwaysVulnerable, SpellId::None, AV_PHASES, 1
};
TEST(always_vulnerable_wound_lands_without_expose){
    BossState b; b.reset(AV_DEF);
    CHECK(b.vulnerable());                        // always vulnerable, no expose needed
    b.on_wound(AV_DEF.wound_dmg);
    CHECK_EQ(b.hp, AV_DEF.max_hp - AV_DEF.wound_dmg);
    b.on_wound(AV_DEF.wound_dmg);                 // blocked by i-frames
    CHECK_EQ(b.hp, AV_DEF.max_hp - AV_DEF.wound_dmg);
    for(int i=0;i<AV_DEF.hit_iframes;++i) b.tick();
    b.on_wound(AV_DEF.wound_dmg);                 // lands again after i-frames -> defeated
    CHECK(b.defeated());
}

// ---------------------------------------------------------------------------
// M12 P1.4 — per-def SWITCH_BUDGET invariant (non-vacuous; see commit notes).
// ---------------------------------------------------------------------------
static void check_budget(const BossDef& d){
    for(int i=0;i<d.phase_count;++i) CHECK(d.phases[i].pattern.telegraph_frames >= SWITCH_BUDGET);
    if(d.vuln==VulnMode::SpellExpose) CHECK(d.expose_frames >= SWITCH_BUDGET);
    if(d.vuln==VulnMode::TiredWindow) CHECK(d.expose_frames >= SWITCH_BUDGET);
}
TEST(switch_budget_holds_for_all_defs){ check_budget(KING_DEF); check_budget(D1_DEF); check_budget(AV_DEF); }

// --- M13 D2 Slagshell (SpellExpose+Fire, pacing, rockfall, long re-armor anti-spam) ---
TEST(bossdef_d2_slagshell_fields){
    CHECK_EQ(D2_DEF.max_hp, 70);
    CHECK_EQ(D2_DEF.wound_dmg, 10);
    CHECK_EQ(D2_DEF.phase_count, 2);
    CHECK((int)D2_DEF.vuln == (int)VulnMode::SpellExpose);
    CHECK((int)D2_DEF.expose_spell == (int)SpellId::Fire);     // Fire melts the crust
    CHECK((int)D2_DEF.locomotion == (int)Locomotion::Pacing);  // the moving boss
    CHECK(D2_DEF.hit_iframes >= 80);                           // LONG re-armor = anti-spam
    CHECK_EQ(D2_DEF.phases[0].end_hp, 35);                     // P1 -> P2 at half HP
    // both phases use AIMED|ROCKFALL; the phase boundary only escalates rockfall intensity
    CHECK_EQ((int)D2_DEF.phases[0].attacks, (int)(BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL));
    CHECK_EQ((int)D2_DEF.phases[1].attacks, (int)(BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL));
    CHECK(D2_DEF.phases[0].pattern.telegraph_frames >= SWITCH_BUDGET);
    CHECK(D2_DEF.phases[1].pattern.telegraph_frames >= SWITCH_BUDGET);
}
// THE anti-spam invariant: after a wound you CANNOT re-expose until hit_iframes drains, and the
// attack pattern resumes (not frozen) during that window. This is what stops Fire->bolt mashing.
TEST(d2_no_reexpose_during_rearm){
    BossState b; b.reset(D2_DEF);
    b.on_expose_hit(SpellId::Fire);  CHECK(b.exposed());        // Fire opens the window
    b.on_wound(D2_DEF.wound_dmg);                               // one wound...
    CHECK_EQ(b.hit_iframes, D2_DEF.hit_iframes);               // ...starts the long re-armor
    CHECK(!b.exposed());                                        // window closed by the wound
    b.on_expose_hit(SpellId::Fire);  CHECK(!b.exposed());       // Fire during re-armor: NO re-expose
    int t = b.attack_timer;
    b.tick();                                                   // pattern RESUMES during re-armor...
    CHECK(b.attack_timer == t + 1 || b.attack_timer == 0);     // (advances, or wraps the period) -> not frozen
    for(int i = 0; i < D2_DEF.hit_iframes; ++i) b.tick();      // drain the re-armor
    CHECK_EQ(b.hit_iframes, 0);
    b.on_expose_hit(SpellId::Fire);  CHECK(b.exposed());        // NOW Fire re-exposes
}
TEST(d2_takes_seven_wounds){
    BossState b; b.reset(D2_DEF);
    int wounds = 0;
    while(!b.defeated() && wounds < 100){
        b.on_expose_hit(SpellId::Fire);
        b.on_wound(D2_DEF.wound_dmg);
        for(int i = 0; i < D2_DEF.hit_iframes; ++i) b.tick();   // wait out re-armor between wounds
        ++wounds;
    }
    CHECK(b.defeated());
    CHECK_EQ(wounds, 7);                                        // 70 / 10
}
// Bolt/None never expose a SpellExpose boss (only the def's expose spell does).
TEST(d2_only_fire_exposes){
    BossState b; b.reset(D2_DEF);
    b.on_expose_hit(SpellId::Ice);   CHECK(!b.exposed());
    b.on_expose_hit(SpellId::Light); CHECK(!b.exposed());
    b.on_expose_hit(SpellId::Fire);  CHECK(b.exposed());
}
// REGRESSION: the new locomotion field must default Stationary for King + D1 (unchanged behaviour).
TEST(king_and_d1_are_stationary){
    CHECK((int)KING_DEF.locomotion == (int)Locomotion::Stationary);
    CHECK((int)D1_DEF.locomotion == (int)Locomotion::Stationary);
}
// M13 QA: D2's red bolts are blockable by Fire; D1 stays dodge-only and the King keeps its own
// local block_player_shots (block_spell defaults None for both — regression on the new field).
TEST(bossdef_block_spell){
    CHECK((int)D2_DEF.block_spell == (int)SpellId::Fire);
    CHECK((int)D1_DEF.block_spell == (int)SpellId::None);
    CHECK((int)KING_DEF.block_spell == (int)SpellId::None);
}
