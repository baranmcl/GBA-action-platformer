#pragma once
namespace logic {
// M8 Stone pound — scene-resolution pure helpers (host-testable; pure C++, no engine types).
// These describe the deterministic geometry of a pound's shockwave so the scene (and the
// no-soft-lock invariant tests) can agree on the same rule.

// A loose platform is a horizontal run [tx .. tx+len-1] at row ty. The Stone pound's shockwave
// reaches any tile within Chebyshev (chessboard) distance <= radius of the impact tile (ix,iy).
// Returns true iff ANY tile of the run is within the shockwave. RADIUS matches GrappleState::RANGE=6.
constexpr int POUND_SHOCKWAVE_RADIUS = 6;

inline int abs_i(int v){ return v < 0 ? -v : v; }

inline bool loose_platform_in_shockwave(int tx, int ty, int len, int ix, int iy,
                                        int radius = POUND_SHOCKWAVE_RADIUS){
    int dy = abs_i(ty - iy);
    if(dy > radius) return false;
    for(int x = tx; x < tx + len; ++x){
        int dx = abs_i(x - ix);
        int cheb = dx > dy ? dx : dy;   // Chebyshev distance
        if(cheb <= radius) return true;
    }
    return false;
}
}
