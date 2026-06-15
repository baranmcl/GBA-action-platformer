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

// --- Variable-length health bar -------------------------------------------------
// The old ratio bar (bar_pixels) rendered 100/100 and 125/125 identically (both a
// full fixed-length bar), so a heart-container's +25 max was invisible. Instead the
// health bar's TOTAL length scales with max: one pip per HP_PER_PIP units of MAX, and
// the fill is one pip per HP_PER_PIP units of CUR. So a bigger max => a LONGER bar.
//
// HP_PER_PIP = 10 keeps the base 100-HP cap at the original 10-pip length; +25 max adds
// ~2-3 pips (125 -> 13, 150 -> 15), a clearly visible lengthening. MAX_HEALTH_PIPS caps
// the sprite count so the bar always fits the HUD's screen space (15 pips from x=-116 at
// 8px each ends at x=-4, on-screen).
static constexpr int HP_PER_PIP        = 10;
static constexpr int MAX_HEALTH_PIPS   = 16;   // fits 150-HP cap (15 pips) + margin

inline int clamp_pips(int n){
    if(n < 0) return 0;
    if(n > MAX_HEALTH_PIPS) return MAX_HEALTH_PIPS;
    return n;
}
// Total length of the health bar (pips) for a given MAX hp. Rounds up so each +25 max
// adds at least one visible pip; clamped to MAX_HEALTH_PIPS.
inline int health_total_pips(int max){
    if(max <= 0) return 0;
    return clamp_pips((max + HP_PER_PIP - 1) / HP_PER_PIP);   // ceil(max/HP_PER_PIP)
}
// Filled portion of the health bar (pips) for the current hp. Rounds up so any non-zero
// HP lights at least one pip; never exceeds the total length for this max.
inline int health_fill_pips(int cur, int max){
    if(cur <= 0) return 0;
    int total = health_total_pips(max);
    int fill  = (cur + HP_PER_PIP - 1) / HP_PER_PIP;          // ceil(cur/HP_PER_PIP)
    if(fill > total) fill = total;
    return fill;
}
}
