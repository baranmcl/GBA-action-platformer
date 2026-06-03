#pragma once
#include <cstdint>
#include "logic/fixed.h"
namespace logic {
enum class TileKind : uint8_t { Empty=0, Solid=1, OneWay=2 };
struct Tilemap {
    int w, h;
    const uint8_t* cells; // row-major, length w*h
    static constexpr int TILE = 8;
    TileKind at(int tx, int ty) const {
        if(tx<0 || ty<0 || tx>=w || ty>=h) return TileKind::Solid;
        return static_cast<TileKind>(cells[ty*w + tx]);
    }
    bool is_solid(int tx,int ty) const { return at(tx,ty)==TileKind::Solid; }
    bool is_oneway(int tx,int ty) const { return at(tx,ty)==TileKind::OneWay; }
    static int px_to_tile(Fixed px){ return px.to_int() / TILE; }
};
}
