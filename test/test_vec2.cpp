#include "test_framework.h"
#include "logic/vec2.h"
using logic::Fixed; using logic::Vec2;
TEST(vec_add){ Vec2 a{Fixed::from_int(1),Fixed::from_int(2)}; Vec2 b{Fixed::from_int(3),Fixed::from_int(4)}; Vec2 c=a+b; CHECK_EQ(c.x.to_int(),4); CHECK_EQ(c.y.to_int(),6); }
TEST(vec_scale){ Vec2 a{Fixed::from_int(2),Fixed::from_int(3)}; Vec2 c=a*Fixed::from_raw(128); CHECK_EQ(c.x.to_int(),1); CHECK_EQ(c.y.to_int(),1); }
