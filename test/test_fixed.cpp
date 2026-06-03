#include "test_framework.h"
#include "logic/fixed.h"
using logic::Fixed;
TEST(fixed_from_int_and_back){ CHECK_EQ(Fixed::from_int(3).to_int(), 3); }
TEST(fixed_add){ CHECK_EQ((Fixed::from_int(2) + Fixed::from_int(5)).raw, Fixed::from_int(7).raw); }
TEST(fixed_fractional_floor){ Fixed a = Fixed::from_raw(1*256 + 128); Fixed b = Fixed::from_raw(1*256 + 192); CHECK_EQ((a+b).to_int(), 3); }
TEST(fixed_mul){ Fixed a = Fixed::from_int(2); Fixed b = Fixed::from_raw(256 + 128); CHECK_EQ((a*b).to_int(), 3); }
TEST(fixed_neg_floor){ CHECK_EQ(Fixed::from_raw(-128).to_int(), -1); }
