#include "logic/collision.h"
namespace logic {
static bool overlaps_solid(const Body& b, const Tilemap& map){
    int l = Tilemap::px_to_tile(b.pos.x);
    int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
    int t = Tilemap::px_to_tile(b.pos.y);
    int bm= Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - Fixed::from_int(1));
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx) if(map.is_solid(tx,ty)) return true;
    return false;
}
void move_and_collide(Body& b, const Tilemap& map){
    b.on_ground = false;
    const Fixed step = Fixed::from_int(Tilemap::TILE); // <= one tile per substep -> no tunneling
    // ---- X axis ----
    Fixed remx = b.vel.x;
    while(remx.raw != 0){
        Fixed d = remx;
        if(d > step) d = step;
        else if(d < -step) d = -step;
        b.pos.x = b.pos.x + d;
        if(overlaps_solid(b,map)){
            Fixed dir = (d.raw>0)? Fixed::from_int(1) : Fixed::from_int(-1);
            // Bounded back-out: a legit overlap is <= one tile; the guard stops an infinite loop
            // if a body is spawned EMBEDDED in solid (backing out along one axis can't clear an
            // overlap on the other axis). The Y-axis / next frame resolves the residual.
            for(int g=0; g<64 && overlaps_solid(b,map); ++g) b.pos.x = b.pos.x - dir;
            b.vel.x = Fixed::from_int(0); break;
        }
        remx = remx - d;
    }
    // ---- Y axis ----
    Fixed remy = b.vel.y;
    while(remy.raw != 0){
        Fixed d = remy;
        if(d > step) d = step;
        else if(d < -step) d = -step;
        Fixed before = b.pos.y;
        b.pos.y = b.pos.y + d;
        bool hit = overlaps_solid(b,map);
        // One-way platforms: land only when moving down AND the body's bottom edge
        // CROSSED the platform's top plane this step (prev above, new below). Snap to
        // rest exactly on top. Checking the crossed plane (not the post-move bottom
        // tile) is what prevents fast falls from skipping the platform.
        if(!hit && d.raw > 0){
            Fixed prev_bottom = before + b.half_h + b.half_h;   // exclusive bottom before step
            Fixed new_bottom  = b.pos.y + b.half_h + b.half_h;  // exclusive bottom after step
            int bm = Tilemap::px_to_tile(new_bottom);           // tile row at the feet
            Fixed plat_top = Fixed::from_int(bm * Tilemap::TILE);
            if(prev_bottom <= plat_top && new_bottom > plat_top){
                int l = Tilemap::px_to_tile(b.pos.x);
                int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
                for(int tx = l; tx <= r && !hit; ++tx){
                    if(map.is_oneway(tx, bm)){
                        hit = true;
                        b.pos.y = plat_top - (b.half_h + b.half_h); // rest on the platform top
                    }
                }
            }
        }
        if(hit){
            Fixed dir = (d.raw>0)? Fixed::from_int(1):Fixed::from_int(-1);
            for(int g=0; g<64 && overlaps_solid(b,map); ++g) b.pos.y = b.pos.y - dir; // bounded back-out
            if(d.raw>0) b.on_ground = true;
            b.vel.y = Fixed::from_int(0);
            break;
        }
        remy = remy - d;
    }
    // Ground probe: when resting or falling (not rising), mark on_ground if a solid or
    // one-way tile sits directly beneath the body's feet. The axis resolver leaves a
    // sub-pixel gap when it backs the body out of overlap, so without this probe
    // on_ground would flicker frame-to-frame. Skipped while moving up so jumping through
    // a one-way platform does not falsely report "grounded".
    if(b.vel.y.raw >= 0){
        int below = Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h); // tile just under the feet
        int l = Tilemap::px_to_tile(b.pos.x);
        int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
        for(int tx=l; tx<=r; ++tx){
            if(map.is_solid(tx,below) || map.is_oneway(tx,below)){ b.on_ground = true; break; }
        }
    }
}
bool aabb_overlap(const Body& a, const Body& b){
    Fixed aL=a.pos.x, aT=a.pos.y, aR=a.pos.x + a.half_w + a.half_w, aB=a.pos.y + a.half_h + a.half_h;
    Fixed bL=b.pos.x, bT=b.pos.y, bR=b.pos.x + b.half_w + b.half_w, bB=b.pos.y + b.half_h + b.half_h;
    return aL < bR && aR > bL && aT < bB && aB > bT;
}
}
