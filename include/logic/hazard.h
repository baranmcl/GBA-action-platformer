#pragma once
#include "logic/physics.h"
#include "logic/tilemap.h"
namespace logic {
// True if any tile the body's AABB overlaps is lava. The scene applies health damage.
inline bool lava_overlap(const Body& b, const Tilemap& map){
    int l = Tilemap::px_to_tile(b.pos.x);
    int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
    int t = Tilemap::px_to_tile(b.pos.y);
    int bm= Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - Fixed::from_int(1));
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx) if(map.is_lava(tx,ty)) return true;
    return false;
}
// True if any tile the body's AABB overlaps is water. The scene applies the same health damage.
inline bool water_overlap(const Body& b, const Tilemap& map){
    int l = Tilemap::px_to_tile(b.pos.x);
    int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
    int t = Tilemap::px_to_tile(b.pos.y);
    int bm= Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - Fixed::from_int(1));
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx) if(map.is_water(tx,ty)) return true;
    return false;
}
// Either damaging hazard (lava or water) — the scene's one damage check.
inline bool hazard_overlap(const Body& b, const Tilemap& map){ return lava_overlap(b,map) || water_overlap(b,map); }
}
