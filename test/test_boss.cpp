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
