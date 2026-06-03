#pragma once
#include "logic/fixed.h"
namespace logic {
struct Vec2 {
    Fixed x, y;
    constexpr Vec2 operator+(Vec2 o) const { return {x+o.x, y+o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x-o.x, y-o.y}; }
    constexpr Vec2 operator*(Fixed s) const { return {x*s, y*s}; }
};
}
