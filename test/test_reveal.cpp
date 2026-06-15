#include "test_framework.h"
#include "logic/reveal.h"
using namespace logic;

TEST(reveal_default_not_revealed){
    RevealState r;
    CHECK(!r.revealed());
    CHECK_EQ(r.timer, 0);
}
TEST(reveal_on_cast_starts_timer){
    RevealState r;
    r.on_cast();
    CHECK(r.revealed());
    CHECK_EQ(r.timer, RevealState::REVEAL_FRAMES);
}
TEST(reveal_tick_decrements_timer){
    RevealState r;
    r.on_cast();
    r.tick();
    CHECK_EQ(r.timer, RevealState::REVEAL_FRAMES - 1);
    CHECK(r.revealed());  // still >0
}
TEST(reveal_revealed_true_while_positive){
    RevealState r;
    r.on_cast();
    // tick down to timer==1: still revealed
    for(int i = 0; i < RevealState::REVEAL_FRAMES - 1; ++i) r.tick();
    CHECK_EQ(r.timer, 1);
    CHECK(r.revealed());
}
TEST(reveal_revealed_false_at_zero){
    RevealState r;
    r.on_cast();
    // tick exactly REVEAL_FRAMES times -> timer reaches 0
    for(int i = 0; i < RevealState::REVEAL_FRAMES; ++i) r.tick();
    CHECK_EQ(r.timer, 0);
    CHECK(!r.revealed());
}
TEST(reveal_tick_does_not_go_negative){
    RevealState r;
    r.tick();  // tick while already at 0
    CHECK_EQ(r.timer, 0);
    CHECK(!r.revealed());
}
TEST(reveal_on_cast_mid_decay_resets_to_full){
    RevealState r;
    r.on_cast();
    // advance partway
    for(int i = 0; i < 50; ++i) r.tick();
    CHECK_EQ(r.timer, RevealState::REVEAL_FRAMES - 50);
    // re-cast should reset to REVEAL_FRAMES
    r.on_cast();
    CHECK_EQ(r.timer, RevealState::REVEAL_FRAMES);
    CHECK(r.revealed());
}
TEST(reveal_frames_constant_is_200){
    // Ensure REVEAL_FRAMES == 200 (spec constant; feel-tunable in QA)
    CHECK_EQ(RevealState::REVEAL_FRAMES, 200);
}
