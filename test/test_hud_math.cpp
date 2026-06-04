#include "test_framework.h"
#include "logic/hud_math.h"
using namespace logic;
TEST(bar_half){ CHECK_EQ(bar_pixels(50,100,64), 32); }
TEST(bar_zero){ CHECK_EQ(bar_pixels(0,100,64), 0); }
TEST(bar_full){ CHECK_EQ(bar_pixels(100,100,64), 64); }
TEST(bar_over_clamps){ CHECK_EQ(bar_pixels(150,100,64), 64); }
TEST(bar_maxzero_safe){ CHECK_EQ(bar_pixels(50,0,64), 0); }
