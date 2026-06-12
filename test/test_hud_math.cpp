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
