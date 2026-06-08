#pragma once
#include "logic/physics.h"
#include "logic/tilemap.h"
namespace logic {
// True if any tile the body's AABB overlaps is an Updraft. The Player rises in it WHILE gliding.
inline bool updraft_overlap(const Body& b, const Tilemap& map){
    int l = Tilemap::px_to_tile(b.pos.x);
    int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
    int t = Tilemap::px_to_tile(b.pos.y);
    int bm= Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - Fixed::from_int(1));
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx) if(map.at(tx,ty)==TileKind::Updraft) return true;
    return false;
}
// -1 if any overlapped tile is WindLeft, +1 if WindRight, 0 if neither (or both cancel).
inline int wind_dir(const Body& b, const Tilemap& map){
    int l = Tilemap::px_to_tile(b.pos.x);
    int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
    int t = Tilemap::px_to_tile(b.pos.y);
    int bm= Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - Fixed::from_int(1));
    int d = 0;
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx){
        TileKind k = map.at(tx,ty);
        if(k==TileKind::WindLeft) d -= 1; else if(k==TileKind::WindRight) d += 1;
    }
    return d<0 ? -1 : d>0 ? 1 : 0;
}
}
