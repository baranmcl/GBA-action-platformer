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
