#include "test_framework.h"
#include "logic/stone.h"
using namespace logic;

TEST(stone_not_active_by_default){
    StoneState s;
    CHECK(!s.active());
    CHECK(!s.just_landed());
    CHECK(!s.invincible());
}
TEST(stone_start_sets_pounding){
    StoneState s;
    s.start();
    CHECK(s.active());
    CHECK(s.invincible());
    CHECK(!s.just_landed()); // landed only set by post()
}
TEST(stone_post_raises_landed_and_ends_on_ground){
    StoneState s;
    s.start();
    s.post(true); // hit ground
    CHECK(s.just_landed());
    CHECK(!s.active()); // pound ended
}
TEST(stone_post_no_landing_while_airborne){
    StoneState s;
    s.start();
    s.post(false); // still in air
    CHECK(s.active());      // still pounding
    CHECK(!s.just_landed()); // no landing signal
}
TEST(stone_invincible_while_pounding){
    StoneState s;
    CHECK(!s.invincible()); // not pounding
    s.start();
    CHECK(s.invincible()); // pounding -> i-frames
}
TEST(stone_landed_is_one_frame){
    // After a landing tick, pound ends; a second post(true) does NOT re-raise landed
    // because pounding is already false.
    StoneState s;
    s.start();
    s.post(true);  // first: pounding=false, landed=true
    CHECK(s.just_landed());
    s.post(true);  // second: pounding already false, so landed stays false
    CHECK(!s.just_landed());
    CHECK(!s.active());
}
TEST(stone_start_idempotent_while_pounding){
    // Calling start() again while already pounding should not re-arm (landed stays false).
    StoneState s;
    s.start();
    s.post(false); // still airborne
    s.start();     // re-arm call while pounding — idempotent
    CHECK(s.active());
}
TEST(stone_pound_vy_raw_constant_is_correct){
    // POUND_VY_RAW == 2048 == Fixed::from_int(8).raw (256 subpixels/px * 8 px)
    // Verify so that player.cpp can trust Fixed::from_raw(StoneState::POUND_VY_RAW).
    CHECK_EQ(StoneState::POUND_VY_RAW, 2048);
}
