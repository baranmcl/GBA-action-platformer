#include "test_framework.h"
#include "logic/stone_impact.h"
using namespace logic;

// Shockwave: a loose platform run is affected iff ANY tile is within Chebyshev distance <= radius (6).

TEST(shockwave_impact_on_platform_tile){
    // impact exactly on the platform's left tile -> affected
    CHECK(loose_platform_in_shockwave(/*tx*/10, /*ty*/5, /*len*/3, /*ix*/10, /*iy*/5));
}
TEST(shockwave_within_radius_diagonal){
    // platform at (10,5)..(12,5); impact at (16,11): nearest tile (12,5) -> dx=4, dy=6, cheb=6 <= 6
    CHECK(loose_platform_in_shockwave(10, 5, 3, 16, 11));
}
TEST(shockwave_just_out_of_radius){
    // nearest tile (12,5); impact at (12,12): dx=0, dy=7, cheb=7 > 6 -> NOT affected
    CHECK(!loose_platform_in_shockwave(10, 5, 3, 12, 12));
}
TEST(shockwave_horizontal_far){
    // platform (10,5)..(10,5) single tile; impact at (17,5): dx=7 > 6 -> not affected
    CHECK(!loose_platform_in_shockwave(10, 5, 1, 17, 5));
}
TEST(shockwave_horizontal_edge){
    // single tile (10,5); impact at (16,5): dx=6 == radius -> affected
    CHECK(loose_platform_in_shockwave(10, 5, 1, 16, 5));
}
TEST(shockwave_run_brings_far_tile_into_range){
    // long run (10..20,5); impact at (20,5) hits the far-right tile exactly -> affected
    CHECK(loose_platform_in_shockwave(10, 5, 11, 20, 5));
}
TEST(shockwave_radius_constant){
    CHECK_EQ(POUND_SHOCKWAVE_RADIUS, 6);
}
