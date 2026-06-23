#include "test_framework.h"
#include "logic/boss.h"
using namespace logic;

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
