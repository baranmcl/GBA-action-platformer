#pragma once
namespace logic {
// Maps a meter value to a bar width in pixels. Pure integer math; safe when
// max<=0 (returns 0, never divides by zero). Lives in the logic layer so it is
// host-testable; the Butano HUD just renders the result.
inline int bar_pixels(int cur, int max, int width){
    if(max <= 0 || width <= 0) return 0;
    if(cur <= 0) return 0;
    if(cur >= max) return width;
    return cur * width / max;
}
}
