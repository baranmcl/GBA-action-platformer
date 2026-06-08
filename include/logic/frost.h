#pragma once
#include "logic/tilemap.h"
namespace logic {
// Reversible terrain: the Ice spell freezes a water tile into a standable platform; the Fire spell
// melts a platform back to water. Each leaves every other tile (incl. regular Solid walls) alone.
inline TileKind ice_freezes(TileKind k){ return k==TileKind::Water       ? TileKind::IcePlatform : k; }
inline TileKind fire_melts (TileKind k){ return k==TileKind::IcePlatform ? TileKind::Water       : k; }
}
