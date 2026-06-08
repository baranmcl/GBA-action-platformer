#pragma once
#include <cstdint>
#include "logic/fixed.h"
namespace logic {
// Water=4 is a damaging hazard the Ice spell freezes into IcePlatform=5 (a standable, Fire-meltable
// solid). IcePlatform counts as solid for collision but is distinct so Fire melts ONLY it, not walls.
enum class TileKind : uint8_t { Empty=0, Solid=1, OneWay=2, Lava=3, Water=4, IcePlatform=5 };
struct Tilemap {
    int w, h;
    const uint8_t* cells; // row-major, length w*h
    static constexpr int TILE = 8;
    TileKind at(int tx, int ty) const {
        if(tx<0 || ty<0 || tx>=w || ty>=h) return TileKind::Solid;
        return static_cast<TileKind>(cells[ty*w + tx]);
    }
    bool is_solid(int tx,int ty) const { TileKind k=at(tx,ty); return k==TileKind::Solid || k==TileKind::IcePlatform; }
    bool is_oneway(int tx,int ty) const { return at(tx,ty)==TileKind::OneWay; }
    bool is_lava(int tx,int ty) const { return at(tx,ty)==TileKind::Lava; }
    bool is_water(int tx,int ty) const { return at(tx,ty)==TileKind::Water; }
    static int px_to_tile(Fixed px){ return px.to_int() / TILE; }
};
}
