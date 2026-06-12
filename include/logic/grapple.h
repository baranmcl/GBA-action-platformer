#pragma once
#include "logic/tilemap.h"
#include "logic/physics.h"
namespace logic {

// Find the nearest GrapplePoint anchor tile within Chebyshev `range` of the player tile,
// in the facing/up aim arc: an anchor is targetable unless it is strictly BEHIND the facing
// direction (anchors ahead, in the same column, or above are all allowed). Nearest by Manhattan
// distance. Returns false if none (out_* untouched on a false return).
inline bool nearest_grapple_anchor(const Tilemap& map, int ptx, int pty, int facing, int range,
                                   int& out_tx, int& out_ty) {
    bool found = false;
    int best = 0;
    for(int ty = pty - range; ty <= pty + range; ++ty)
    for(int tx = ptx - range; tx <= ptx + range; ++tx){
        if(!map.is_grapple_point(tx, ty)) continue;
        int sx = (tx > ptx) - (tx < ptx);            // -1/0/+1 horizontal sign
        if(sx == -facing && tx != ptx) continue;     // exclude strictly-behind anchors (sx==0: same column, never excluded)
        int dist = (tx<ptx?ptx-tx:tx-ptx) + (ty<pty?pty-ty:ty-pty); // Manhattan
        if(!found || dist < best){ found = true; best = dist; out_tx = tx; out_ty = ty; }
    }
    return found;
}

struct GrappleState {
    static constexpr int RANGE = 6;                          // tiles (feel-tunable)
    static constexpr Fixed GRAPPLE_VX = Fixed::from_int(5);  // ~5 px/frame per-axis pull (feel-tunable)
    bool pulling = false;
    int anchor_tx = 0, anchor_ty = 0;                        // latched anchor tile

    bool active() const { return pulling; }

    // On a fire edge with the ability and an anchor in range, latch onto the nearest anchor.
    bool latch(bool fire, const Body& body, int facing, const Tilemap& map, bool has_grapple){
        if(!fire || pulling || !has_grapple) return false;
        int ptx = Tilemap::px_to_tile(body.pos.x + body.half_w);   // scan from the body CENTRE tile
        int pty = Tilemap::px_to_tile(body.pos.y + body.half_h);
        if(!nearest_grapple_anchor(map, ptx, pty, facing, RANGE, anchor_tx, anchor_ty)) return false;
        pulling = true; return true;
    }

    // Velocity toward the anchor tile centre, per-axis clamped to ±GRAPPLE_VX (converges near it).
    Vec2 pull_velocity(const Body& body) const {
        Fixed cx = body.pos.x + body.half_w, cy = body.pos.y + body.half_h;
        Fixed tx = Fixed::from_int(anchor_tx*8 + 4), ty = Fixed::from_int(anchor_ty*8 + 4);
        return { clamp_axis(tx - cx), clamp_axis(ty - cy) };
    }

    // End the pull when the body's CENTRE TILE reaches the anchor tile (tile-level arrival — robust
    // even when a floor prevents exact vertical alignment), or when blocked (the body didn't move).
    // On arrival: snap the player standing on the first solid tile at/below the anchor column and
    // zero velocity so no residual pull carries them past (e.g. into a lava pit).
    // On wall-stop (!moved): zero velocity and end (kill residual; no position snap).
    void post(Body& body, const Tilemap& map, bool moved){
        int ctx = Tilemap::px_to_tile(body.pos.x + body.half_w);
        int cty = Tilemap::px_to_tile(body.pos.y + body.half_h);
        if(ctx == anchor_tx && cty == anchor_ty){
            // Arrived at anchor tile: snap onto the platform directly below.
            int fy = anchor_ty;
            for(; fy < map.h; ++fy){
                if(map.is_solid(anchor_tx, fy)) break;
            }
            if(fy < map.h){
                body.pos.x = Fixed::from_int(anchor_tx*8 + 4) - body.half_w;
                body.pos.y = Fixed::from_int(fy*8) - body.half_h - body.half_h;
            }
            body.vel = Vec2{ Fixed::from_int(0), Fixed::from_int(0) };
            pulling = false;
        } else if(!moved){
            // Blocked before arrival: kill residual velocity so no slide/fling.
            body.vel = Vec2{ Fixed::from_int(0), Fixed::from_int(0) };
            pulling = false;
        }
    }
private:
    static Fixed clamp_axis(Fixed d){
        if(d > GRAPPLE_VX) return GRAPPLE_VX;
        if(d < -GRAPPLE_VX) return -GRAPPLE_VX;     // Fixed has unary operator-
        return d;
    }
};

}
