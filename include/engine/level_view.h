#pragma once
#include "bn_regular_bg_ptr.h"
#include "logic/tilemap.h"
namespace engine {
// A rendered level: the Butano background plus the map's pixel dimensions, which
// the avatar/camera need for the level-space -> world-space transform.
struct LevelView {
    bn::regular_bg_ptr bg;
    int map_px_w;
    int map_px_h;
};
// Builds a dynamic background mirroring `map` (logic tile ints map 1:1 to bg tile
// indices: 0 blank, 1 ground, 2 one-way, 3 gate, 4 cage). The map is placed at the
// top-left of a fixed 64x32-tile background; cells outside `map` are blank.
LevelView build_level_view(const logic::Tilemap& map);
}
