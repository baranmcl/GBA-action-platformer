#pragma once
#include "logic/tilemap.h"
namespace logic {

// Find the nearest GrapplePoint anchor tile within Chebyshev `range` of the player tile,
// in the facing/up aim arc: an anchor is targetable unless it is strictly BEHIND the facing
// direction (anchors ahead, in the same column, or above are all allowed). Nearest by Manhattan
// distance. Returns false if none (out_* untouched on a false return).
inline bool nearest_grapple_anchor(const Tilemap& map, int ptx, int pty, int facing, int range,
                                   int& out_tx, int& out_ty){
    bool found = false; int best = 0;
    for(int ty = pty - range; ty <= pty + range; ++ty)
    for(int tx = ptx - range; tx <= ptx + range; ++tx){
        if(!map.is_grapple_point(tx, ty)) continue;
        int sx = (tx > ptx) - (tx < ptx);            // -1/0/+1 horizontal sign
        if(sx == -facing && tx != ptx) continue;     // exclude strictly-behind anchors
        int dist = (tx<ptx?ptx-tx:tx-ptx) + (ty<pty?pty-ty:ty-pty); // Manhattan
        if(!found || dist < best){ found = true; best = dist; out_tx = tx; out_ty = ty; }
    }
    return found;
}
}
