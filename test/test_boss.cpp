#include "test_framework.h"
#include "logic/boss.h"
using namespace logic;

// Land exactly one wound the way the fight now requires: run out any post-wound i-frames, expose with
// Light, then wound. (A wound ends the expose + grants i-frames, so wounds must be paced like this.)
static void wound_once(BossState& b){
    for(int i = 0; i < HIT_IFRAMES + 1; ++i) b.tick();   // clear post-wound i-frames
    b.on_light_hit();                                    // expose
    b.on_wound(WOUND_DMG);                               // one wound (ends expose, sets i-frames)
}

TEST(boss_initial_state){
    BossState b;
    b.reset(); // sets P1 + full HP + cleared timers
    CHECK_EQ((int)b.phase, (int)BossPhase::P1);
    CHECK_EQ(b.hp, KING_MAX_HP);
    CHECK_EQ(b.phase_start_hp, KING_MAX_HP);
    CHECK_EQ(b.expose_timer, 0);
    CHECK(!b.exposed());
    CHECK(!b.defeated());
}

TEST(boss_constants_sane){
    CHECK_EQ(KING_MAX_HP, 90);
    CHECK(P1_END_HP > P2_END_HP);
    CHECK(P2_END_HP > 0);
    CHECK_EQ(KING_MAX_HP % WOUND_DMG, 0); // HP is a whole number of wounds
}

TEST(boss_light_hit_exposes){
    BossState b; b.reset();
    b.on_light_hit();
    CHECK(b.exposed());
    CHECK_EQ((int)b.phase, (int)BossPhase::Exposed);
    CHECK_EQ((int)b.exposed_return, (int)BossPhase::P1); // remembers underlying phase
    CHECK_EQ(b.expose_timer, EXPOSE_FRAMES);
}

TEST(boss_light_hit_while_already_exposed_refreshes){
    BossState b; b.reset();
    b.on_light_hit();
    b.tick(); // expose_timer 75 -> 74
    b.on_light_hit(); // refresh, do NOT overwrite exposed_return with Exposed
    CHECK_EQ(b.expose_timer, EXPOSE_FRAMES);
    CHECK_EQ((int)b.exposed_return, (int)BossPhase::P1);
}

TEST(boss_expose_decays_to_exact_zero_then_returns){
    BossState b; b.reset();
    b.on_light_hit();                       // expose_timer = 75, phase = Exposed
    for(int i = 0; i < EXPOSE_FRAMES - 1; ++i) b.tick();
    CHECK(b.exposed());                      // still >0 at frame 74
    CHECK_EQ(b.expose_timer, 1);
    CHECK_EQ((int)b.phase, (int)BossPhase::Exposed);
    b.tick();                                // 1 -> 0
    CHECK(!b.exposed());
    CHECK_EQ((int)b.phase, (int)BossPhase::P1); // returned to exposed_return
}

TEST(boss_attack_timer_advances_and_wraps_per_phase){
    BossState b; b.reset();                  // phase P1
    int period = PHASE_PATTERNS[0].telegraph_frames
               + PHASE_PATTERNS[0].attack_active_frames
               + PHASE_PATTERNS[0].recovery_frames;
    for(int i = 0; i < period; ++i) b.tick();
    CHECK_EQ(b.attack_timer, 0);             // wrapped exactly at the phase period
}

TEST(boss_attack_frozen_while_exposed){
    // The expose window is a CLEAN damage window — the King's attack pattern must NOT
    // advance while EXPOSED (so the player can safely switch spells + wound; co-design rule).
    BossState b; b.reset();
    for(int i = 0; i < 5; ++i) b.tick();     // advance the attack pattern a little
    int frozen = b.attack_timer;
    b.on_light_hit();                        // EXPOSED
    for(int i = 0; i < 10; ++i) b.tick();    // ticks during expose
    CHECK_EQ(b.attack_timer, frozen);        // unchanged while exposed
}

TEST(boss_wound_only_while_exposed){
    BossState b; b.reset();
    b.on_wound(WOUND_DMG);          // NOT exposed -> no effect (shielded)
    CHECK_EQ(b.hp, KING_MAX_HP);
    b.on_light_hit();
    b.on_wound(WOUND_DMG);          // exposed -> subtract
    CHECK_EQ(b.hp, KING_MAX_HP - WOUND_DMG);
}

TEST(boss_wound_ends_expose_and_sets_iframes){
    BossState b; b.reset();
    b.on_light_hit();
    CHECK(b.exposed());
    b.on_wound(WOUND_DMG);
    CHECK(!b.exposed());                       // a wound ends the expose window
    CHECK_EQ(b.hit_iframes, HIT_IFRAMES);
    CHECK_EQ(b.hp, KING_MAX_HP - WOUND_DMG);
}

TEST(boss_one_wound_per_expose){
    BossState b; b.reset();
    b.on_light_hit();
    b.on_wound(WOUND_DMG);                     // lands
    b.on_wound(WOUND_DMG);                     // blocked (expose ended + i-frames)
    CHECK_EQ(b.hp, KING_MAX_HP - WOUND_DMG);
}

TEST(boss_cannot_reexpose_during_iframes){
    BossState b; b.reset();
    b.on_light_hit(); b.on_wound(WOUND_DMG);   // now in i-frames
    b.on_light_hit();                          // blocked while i-frames > 0
    CHECK(!b.exposed());
    for(int i = 0; i < HIT_IFRAMES; ++i) b.tick();   // i-frames decay
    CHECK_EQ(b.hit_iframes, 0);
    b.on_light_hit();                          // works again
    CHECK(b.exposed());
}

TEST(boss_wound_crossing_threshold_advances_phase){
    BossState b; b.reset();
    while(b.hp > P1_END_HP) wound_once(b);     // 90 -> 60 (crosses), one wound per Light
    CHECK_EQ(b.hp, P1_END_HP);
    CHECK_EQ((int)b.exposed_return, (int)BossPhase::P2); // underlying phase advanced
    CHECK_EQ(b.phase_start_hp, P1_END_HP);
}

TEST(boss_wound_to_zero_defeats){
    BossState b; b.reset();
    int guard = 0;
    while(!b.defeated() && guard++ < 100) wound_once(b);
    CHECK(b.defeated());
    CHECK_EQ((int)b.phase, (int)BossPhase::Defeated);
    CHECK_EQ(guard, KING_MAX_HP / WOUND_DMG);  // exactly 9 wounds (one per Light) to kill
}

TEST(boss_player_death_restarts_full_fight){
    BossState b; b.reset();
    while(b.hp > P2_END_HP) wound_once(b);     // deep into the fight (P3)
    b.on_player_death();
    CHECK_EQ((int)b.phase, (int)BossPhase::P1);
    CHECK_EQ(b.hp, KING_MAX_HP);
    CHECK_EQ(b.phase_start_hp, KING_MAX_HP);
    CHECK_EQ(b.expose_timer, 0);
    CHECK_EQ(b.attack_timer, 0);
    CHECK_EQ(b.hit_iframes, 0);
    CHECK(!b.exposed());
}

TEST(switch_budget_invariant_all_windows){
    // No telegraph window and no expose window may be tighter than the worst-case
    // time to cycle (L) to the right spell + react + cast. If this fails, a window
    // was set below SWITCH_BUDGET -> the phase would be unfair regardless of skill.
    for(int i = 0; i < 3; ++i)
        CHECK(PHASE_PATTERNS[i].telegraph_frames >= SWITCH_BUDGET);
    CHECK(EXPOSE_FRAMES >= SWITCH_BUDGET);
}

TEST(phase_pattern_escalates_but_respects_budget){
    // Telegraph shrinks each phase (escalation) but never below the floor.
    CHECK(PHASE_PATTERNS[0].telegraph_frames >= PHASE_PATTERNS[1].telegraph_frames);
    CHECK(PHASE_PATTERNS[1].telegraph_frames >= PHASE_PATTERNS[2].telegraph_frames);
    CHECK(PHASE_PATTERNS[2].telegraph_frames >= SWITCH_BUDGET);
}

TEST(boss_phase_step_is_deterministic){
    // Same phase + same attack_timer -> same pattern step (no hidden state/RNG).
    BossState a; a.reset(); BossState b; b.reset();
    for(int i=0;i<40;++i){ a.tick(); b.tick(); }
    CHECK_EQ(a.attack_timer, b.attack_timer);
    CHECK_EQ((int)a.current_step(), (int)b.current_step());
}
