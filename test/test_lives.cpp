#include "test_framework.h"
#include "logic/world_state.h"
using namespace logic;

// --- max_lives ---

TEST(max_lives_zero_spronks){
    World w;
    CHECK_EQ(max_lives(w), 3);
}

TEST(max_lives_one_spronk){
    World w;
    w.free_spronk(1);
    CHECK_EQ(max_lives(w), 4);
}

TEST(max_lives_three_spronks){
    World w;
    w.free_spronk(1); w.free_spronk(2); w.free_spronk(3);
    CHECK_EQ(max_lives(w), 6);
}

TEST(max_lives_all_eight_spronks){
    World w;
    for(int d = 1; d <= 8; ++d) w.free_spronk(d);
    CHECK_EQ(max_lives(w), 11);
}

// --- STARTING_LIVES and default lives ---

TEST(starting_lives_constant_is_3){
    CHECK_EQ((int)World::STARTING_LIVES, 3);
}

TEST(world_default_lives_is_starting){
    World w;
    CHECK_EQ((int)w.lives, (int)World::STARTING_LIVES);
    CHECK_EQ((int)w.lives, 3);
}

// --- lose_life ---

TEST(lose_life_decrements){
    World w;
    // lives starts at 3
    lose_life(w);
    CHECK_EQ((int)w.lives, 2);
    lose_life(w);
    CHECK_EQ((int)w.lives, 1);
}

TEST(lose_life_clamps_at_zero){
    World w;
    w.lives = 1;
    lose_life(w);
    CHECK_EQ((int)w.lives, 0);
    // calling lose_life at 0 must not wrap / underflow
    lose_life(w);
    CHECK_EQ((int)w.lives, 0);
}

TEST(lose_life_already_at_zero_stays_zero){
    World w;
    w.lives = 0;
    lose_life(w);
    CHECK_EQ((int)w.lives, 0);
}

// --- death boundary: three lives -> three loses -> 2, 1, 0 ---

TEST(death_boundary_three_lives_to_game_over){
    World w;
    // fresh game: 3 lives
    CHECK_EQ((int)w.lives, 3);
    lose_life(w); CHECK_EQ((int)w.lives, 2);
    lose_life(w); CHECK_EQ((int)w.lives, 1);
    lose_life(w); CHECK_EQ((int)w.lives, 0); // this is the Game-Over trigger
}

// --- refill_lives ---

TEST(refill_lives_no_spronks){
    World w;
    w.lives = 1;
    refill_lives(w);
    CHECK_EQ((int)w.lives, 3); // max_lives with 0 spronks = 3
}

TEST(refill_lives_two_spronks){
    World w;
    w.free_spronk(1); w.free_spronk(2);
    w.lives = 0;
    refill_lives(w);
    CHECK_EQ((int)w.lives, 5); // 3 + 2 = 5
}

TEST(refill_lives_already_at_max_unchanged){
    World w;
    // 3 lives, max is 3
    refill_lives(w);
    CHECK_EQ((int)w.lives, 3);
}

// --- clamp_lives_on_load ---

TEST(clamp_lives_on_load_zero_becomes_max){
    World w;
    w.lives = 0;
    clamp_lives_on_load(w);
    CHECK_EQ((int)w.lives, max_lives(w)); // 3 with 0 spronks
}

TEST(clamp_lives_on_load_above_max_becomes_max){
    World w;
    // max is 3 (no spronks); set lives to 10
    w.lives = 10;
    clamp_lives_on_load(w);
    CHECK_EQ((int)w.lives, 3);
}

TEST(clamp_lives_on_load_within_range_unchanged){
    World w;
    w.free_spronk(1); // max = 4
    w.lives = 2;
    clamp_lives_on_load(w);
    CHECK_EQ((int)w.lives, 2);
}

TEST(clamp_lives_on_load_at_max_unchanged){
    World w;
    w.free_spronk(1); // max = 4
    w.lives = 4;
    clamp_lives_on_load(w);
    CHECK_EQ((int)w.lives, 4);
}

// --- spronk_count ---

TEST(spronk_count_zero){
    World w;
    CHECK_EQ(spronk_count(w), 0);
}

TEST(spronk_count_three){
    World w;
    w.free_spronk(2); w.free_spronk(5); w.free_spronk(8);
    CHECK_EQ(spronk_count(w), 3);
}
