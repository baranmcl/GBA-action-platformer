#include "test_framework.h"
#include "logic/physics.h"
using namespace logic;
TEST(gravity_accumulates){ Body b{}; b.vel.y=Fixed::from_int(0); PhysicsParams p; p.gravity=Fixed::from_raw(40); p.terminal=Fixed::from_int(6); apply_gravity(b,p); CHECK_EQ(b.vel.y.raw, 40); }
TEST(gravity_caps_at_terminal){ Body b{}; b.vel.y=Fixed::from_int(6); PhysicsParams p; p.gravity=Fixed::from_raw(40); p.terminal=Fixed::from_int(6); apply_gravity(b,p); CHECK_EQ(b.vel.y.raw, Fixed::from_int(6).raw); }
