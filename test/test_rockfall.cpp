#include "test_framework.h"
#include "logic/rockfall.h"
using namespace logic;

static bool has_gap(const int* cols, int n, int w, int gap_min){
    // a rock-free run of >= gap_min interior columns [1, w-2] must exist
    int run = 0;
    for(int x = 1; x <= w - 2; ++x){
        bool rock = false; for(int i = 0; i < n; ++i) if(cols[i] == x) rock = true;
        run = rock ? 0 : run + 1;
        if(run >= gap_min) return true;
    }
    return false;
}

TEST(rockfall_count_bounds_distinct_sorted){
    int out[8];
    int n = rockfall_columns(/*player_tx=*/20, /*arena_w=*/48, /*count=*/5, /*seed=*/0, out, 8);
    CHECK_EQ(n, 5);
    for(int i = 0; i < n; ++i){ CHECK(out[i] >= 1); CHECK(out[i] <= 46); }   // inside the walls
    for(int i = 1; i < n; ++i){ CHECK(out[i] > out[i-1]); }                  // sorted, distinct
}
TEST(rockfall_always_leaves_a_dodge_lane){
    int out[8];
    for(int seed = 0; seed < 8; ++seed){
        for(int ptx = 1; ptx <= 46; ++ptx){
            int n = rockfall_columns(ptx, 48, 5, seed, out, 8);
            CHECK(has_gap(out, n, 48, ROCKFALL_GAP_MIN));   // never walls off the arena
        }
    }
}
TEST(rockfall_biases_one_rock_to_player){
    int out[8];
    int n = rockfall_columns(/*player_tx=*/12, 48, 3, /*seed=*/2, out, 8);
    bool near = false; for(int i = 0; i < n; ++i) if(out[i] >= 10 && out[i] <= 14) near = true;
    CHECK(near);   // a rock falls at/near the player's column
}
TEST(rockfall_seed_varies_layout){
    int a[8], bb[8];
    int na = rockfall_columns(20, 48, 5, 0, a, 8);
    int nb = rockfall_columns(20, 48, 5, 1, bb, 8);
    CHECK_EQ(na, nb);
    bool differ = false; for(int i = 0; i < na; ++i) if(a[i] != bb[i]) differ = true;
    CHECK(differ);   // different seed -> different spread (not a static pattern)
}
TEST(rockfall_clamps_count_to_max_out){
    int out[3];
    int n = rockfall_columns(20, 48, 5, 0, out, 3);
    CHECK_EQ(n, 3);   // never writes past max_out
}
