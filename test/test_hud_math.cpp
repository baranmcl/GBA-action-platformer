#include "test_framework.h"
#include "logic/hud_math.h"
#include "logic/world_state.h"
using namespace logic;
TEST(bar_half){ CHECK_EQ(bar_pixels(50,100,64), 32); }
TEST(bar_zero){ CHECK_EQ(bar_pixels(0,100,64), 0); }
TEST(bar_full){ CHECK_EQ(bar_pixels(100,100,64), 64); }
TEST(bar_over_clamps){ CHECK_EQ(bar_pixels(150,100,64), 64); }
TEST(bar_maxzero_safe){ CHECK_EQ(bar_pixels(50,0,64), 0); }

// --- variable max health: the HUD draws a fixed PIPS=10 bar whose FILL scales with max,
// so a >100 max (heart-container upgrades) renders correctly with no overflow/clipping. ---
static constexpr int PIPS = 10;   // mirrors engine::Hud::PIPS
TEST(bar_max150_full){
    // 150/150 -> all 10 pips lit (full), never more than PIPS.
    CHECK_EQ(bar_pixels(150,150,PIPS), PIPS);
}
TEST(bar_max150_half){
    // 75/150 -> 5 of 10 pips (the bar is a RATIO, so a bigger max still maps onto 10 pips).
    CHECK_EQ(bar_pixels(75,150,PIPS), 5);
}
TEST(bar_max150_partial){
    // 120/150 -> 8 pips; correct proportional fill for a 150 cap.
    CHECK_EQ(bar_pixels(120,150,PIPS), 8);
}
TEST(bar_max100_baseline){
    // Regression: base 100 cap still renders the full bar at 100 and half at 50.
    CHECK_EQ(bar_pixels(100,100,PIPS), PIPS);
    CHECK_EQ(bar_pixels(50,100,PIPS), 5);
}
TEST(bar_never_exceeds_pips){
    // Even an out-of-range cur can't push the pip count past PIPS (no sprite overflow).
    CHECK_EQ(bar_pixels(999,150,PIPS), PIPS);
}

// --- max_health_for ties the cap to collected heart containers; the HUD then scales to it. ---
TEST(hud_scales_to_one_container){
    World w; w.collect_heart_container(0);
    CHECK_EQ(max_health_for(w), 125);
    CHECK_EQ(bar_pixels(125,max_health_for(w),PIPS), PIPS);   // full bar at the new cap
}
TEST(hud_scales_to_two_containers){
    World w; w.collect_heart_container(0); w.collect_heart_container(1);
    CHECK_EQ(max_health_for(w), 150);
    CHECK_EQ(bar_pixels(150,max_health_for(w),PIPS), PIPS);
    CHECK_EQ(bar_pixels(75, max_health_for(w),PIPS), 5);
}

// --- Variable-length health bar: the bar's TOTAL pip count scales with MAX, so a heart
// container makes it VISIBLY longer (ratio bars rendered 100/100 and 125/125 identically). ---
TEST(health_bar_base_length){
    CHECK_EQ(health_total_pips(100), 10);            // base cap == original 10-pip length
}
TEST(health_bar_grows_with_max){
    // The key bug fix: a bigger MAX yields MORE total pips (a longer bar), not the same length.
    CHECK_EQ(health_total_pips(125), 13);            // +25 max -> +3 pips (ceil(125/10))
    CHECK_EQ(health_total_pips(150), 15);
    CHECK(health_total_pips(125) > health_total_pips(100));
    CHECK(health_total_pips(150) > health_total_pips(125));
}
TEST(health_bar_full_at_longer_length){
    // 125/125 is full at the LONGER length (13 lit), vs 100/100 full at 10 lit -> visibly longer.
    CHECK_EQ(health_fill_pips(125,125), 13);
    CHECK_EQ(health_fill_pips(100,100), 10);
    CHECK(health_fill_pips(125,125) > health_fill_pips(100,100));
}
TEST(health_bar_partial_at_longer_length){
    // 100/125 is partially filled on the 13-pip bar (10 of 13 lit) -> not full, shows headroom.
    CHECK_EQ(health_fill_pips(100,125), 10);
    CHECK(health_fill_pips(100,125) < health_total_pips(125));
}
TEST(health_bar_fill_never_exceeds_total){
    CHECK_EQ(health_fill_pips(999,125), 13);         // clamps to the bar's length
    CHECK_EQ(health_fill_pips(150,150), 15);
}
TEST(health_bar_zero_and_caps){
    CHECK_EQ(health_fill_pips(0,125), 0);            // empty
    CHECK_EQ(health_total_pips(0), 0);               // safe at max<=0
    CHECK(health_total_pips(10000) <= MAX_HEALTH_PIPS);  // capped to fit HUD screen space
}
TEST(health_bar_ties_to_containers){
    World w; w.collect_heart_container(0);
    CHECK_EQ(health_total_pips(max_health_for(w)), 13);          // 125 cap -> 13-pip bar
    CHECK_EQ(health_fill_pips(125, max_health_for(w)), 13);      // full at the longer length
    CHECK_EQ(health_fill_pips(100, max_health_for(w)), 10);      // partial on the longer bar
}

// --- lives_display_count: clamps to [0, LIVES_HUD_CAP], floors negatives at 0 ---
TEST(lives_display_zero){
    CHECK_EQ(lives_display_count(0), 0);
}
TEST(lives_display_one){
    CHECK_EQ(lives_display_count(1), 1);
}
TEST(lives_display_three_starting){
    CHECK_EQ(lives_display_count(3), 3);    // STARTING_LIVES = 3
}
TEST(lives_display_at_cap){
    CHECK_EQ(lives_display_count(LIVES_HUD_CAP), LIVES_HUD_CAP);
}
TEST(lives_display_above_cap_clamped){
    CHECK_EQ(lives_display_count(15), LIVES_HUD_CAP);   // 15 > 12 -> clamp to cap
    CHECK_EQ(lives_display_count(LIVES_HUD_CAP + 1), LIVES_HUD_CAP);
}
TEST(lives_display_negative_floors_to_zero){
    CHECK_EQ(lives_display_count(-1), 0);
    CHECK_EQ(lives_display_count(-99), 0);
}
TEST(lives_display_explicit_cap){
    // custom cap: lives_display_count(lives, cap) honours the cap arg
    CHECK_EQ(lives_display_count(5, 3), 3);
    CHECK_EQ(lives_display_count(2, 3), 2);
}
