#include "test_framework.h"
#include "logic/world_state.h"
using namespace logic;

// --- make_debug_world (I35 dev tooling) ---
// Pure helper behind game::run_debug_select's debug-menu World construction. The bn::-coupled
// menu scene itself isn't host-tested (see CLAUDE.md's three-layer rule); this covers the pure
// logic it delegates to.

TEST(make_debug_world_no_abilities_no_spronks){
    World w = make_debug_world(0, 0);
    CHECK_EQ((int)w.abilities, 0);
    CHECK_EQ(spronk_count(w), 0);
    CHECK_EQ((int)w.lives, 3); // max_lives with 0 spronks
}

TEST(make_debug_world_sets_ability_mask_directly){
    uint16_t mask = (uint16_t)(1u << (int)Ability::Fire) | (uint16_t)(1u << (int)Ability::Grapple);
    World w = make_debug_world(mask, 0);
    CHECK(w.has(Ability::Fire));
    CHECK(w.has(Ability::Grapple));
    CHECK(!w.has(Ability::Ice));
    CHECK(!w.has(Ability::Featherleap));
}

TEST(make_debug_world_all_abilities){
    uint16_t mask = 0;
    for(int i = 0; i < 8; ++i) mask |= (uint16_t)(1u << i);
    World w = make_debug_world(mask, 0);
    CHECK(w.has(Ability::Featherleap));
    CHECK(w.has(Ability::Fire));
    CHECK(w.has(Ability::Ice));
    CHECK(w.has(Ability::Glide));
    CHECK(w.has(Ability::Dash));
    CHECK(w.has(Ability::Grapple));
    CHECK(w.has(Ability::Stone));
    CHECK(w.has(Ability::Light));
}

TEST(make_debug_world_frees_spronks_1_through_n){
    World w = make_debug_world(0, 3);
    CHECK(w.spronk_freed(1));
    CHECK(w.spronk_freed(2));
    CHECK(w.spronk_freed(3));
    CHECK(!w.spronk_freed(4));
    CHECK_EQ(spronk_count(w), 3);
}

TEST(make_debug_world_all_eight_spronks){
    World w = make_debug_world(0, 8);
    CHECK_EQ(spronk_count(w), 8);
    for(int d = 1; d <= 8; ++d) CHECK(w.spronk_freed(d));
}

TEST(make_debug_world_spronks_grant_matching_lives){
    World w = make_debug_world(0, 5);
    CHECK_EQ((int)w.lives, max_lives(w)); // refilled
    CHECK_EQ((int)w.lives, 8);            // 3 + 5
}

TEST(make_debug_world_clamps_spronk_count_above_8){
    World w = make_debug_world(0, 20);
    CHECK_EQ(spronk_count(w), 8);
}

TEST(make_debug_world_clamps_negative_spronk_count){
    World w = make_debug_world(0, -1);
    CHECK_EQ(spronk_count(w), 0);
}

TEST(make_debug_world_current_dungeon_left_at_zero){
    World w = make_debug_world(0, 4);
    CHECK_EQ((int)w.current_dungeon, 0); // caller (main.cpp) sets this before launching
}

TEST(make_debug_world_door_gating_matches_spronk_count){
    World w = make_debug_world(0, 4);
    // door_enterable mirrors a real playthrough that freed the same spronks
    CHECK(door_enterable(1, w));
    CHECK(door_enterable(5, w)); // needs spronk 4
    CHECK(!door_enterable(6, w)); // needs spronk 5, not freed
}
