#include "engine/level_loader.h"
#include "bn_common.h"               // BN_DATA_EWRAM_BSS
namespace engine {
namespace {
    constexpr int CAP = 64 * 128;    // max level size (matches level_view 64x128 big-map bg); 8KB
    BN_DATA_EWRAM_BSS uint8_t s_grid[CAP]; // MUTABLE collision buffer in EWRAM (constexpr LevelData.tiles is read-only)
    int s_w = 0, s_h = 0;
}

LoadedLevel load_level(const logic::LevelData& level){
    s_w = level.w; s_h = level.h;
    for(int i = 0; i < CAP; ++i) s_grid[i] = 0;
    for(int y = 0; y < level.h; ++y)
        for(int x = 0; x < level.w; ++x)
            s_grid[y * level.w + x] = level.tiles[y * level.w + x];
    logic::Tilemap map{ level.w, level.h, s_grid };
    LevelView view = build_level_view(map);
    return LoadedLevel{ map, view };
}

void set_collision_tile(int tx, int ty, uint8_t v){
    if(tx < 0 || ty < 0 || tx >= s_w || ty >= s_h) return;
    s_grid[ty * s_w + tx] = v;
}
}
