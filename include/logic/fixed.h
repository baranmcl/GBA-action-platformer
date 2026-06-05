#pragma once
#include <cstdint>
namespace logic {
struct Fixed {
    int32_t raw = 0; // 16.8 fixed point (default-zero so Vec2/Body never start indeterminate)
    static constexpr int SHIFT = 8;
    static constexpr Fixed from_raw(int32_t r){ return Fixed{r}; }
    static constexpr Fixed from_int(int32_t i){ return Fixed{ i << SHIFT }; }
    constexpr int32_t to_int() const { return raw >> SHIFT; }
    constexpr Fixed operator+(Fixed o) const { return {raw + o.raw}; }
    constexpr Fixed operator-(Fixed o) const { return {raw - o.raw}; }
    constexpr Fixed operator-() const { return {-raw}; }
    constexpr Fixed operator*(Fixed o) const { return { (int32_t)(((int64_t)raw * o.raw) >> SHIFT) }; }
    constexpr bool operator==(Fixed o) const { return raw == o.raw; }
    constexpr bool operator<(Fixed o) const { return raw < o.raw; }
    constexpr bool operator>(Fixed o) const { return raw > o.raw; }
    constexpr bool operator<=(Fixed o) const { return raw <= o.raw; }
    constexpr bool operator>=(Fixed o) const { return raw >= o.raw; }
};
inline constexpr Fixed operator""_fx(unsigned long long i){ return Fixed::from_int((int32_t)i); }
}
