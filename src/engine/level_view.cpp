#include "engine/level_view.h"
#include "bn_size.h"
#include "bn_memory.h"
#include "bn_bg_tiles.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_map_cell_info.h"
#include "bn_regular_bg_tiles_items_tiles.h"
#include "bn_bg_palette_items_bg_palette.h"

namespace engine {
namespace {
    constexpr int COLS = 64;
    constexpr int ROWS = 32;
    // Static storage (no heap — Butano forbids it). 64*32 cells = 4KB.
    alignas(int) bn::regular_bg_map_cell s_cells[COLS * ROWS];
    bn::regular_bg_map_item s_map_item(s_cells[0], bn::size(COLS, ROWS));
}

LevelView build_level_view(const logic::Tilemap& map){
    bn::memory::clear(s_cells);
    for(int ty = 0; ty < ROWS; ++ty){
        for(int tx = 0; tx < COLS; ++tx){
            int tile_index = 0;
            if(tx < map.w && ty < map.h){
                tile_index = map.cells[ty * map.w + tx]; // 0..4 -> bg tile index
            }
            int ci = s_map_item.cell_index(tx, ty);
            bn::regular_bg_map_cell_info info(s_cells[ci]);
            info.set_tile_index(tile_index);
            info.set_palette_id(0);
            info.set_horizontal_flip(false);
            info.set_vertical_flip(false);
            s_cells[ci] = info.cell();
        }
    }
    bn::bg_tiles::set_allow_offset(false);
    bn::regular_bg_item item(bn::regular_bg_tiles_items::tiles,
                             bn::bg_palette_items::bg_palette, s_map_item);
    bn::regular_bg_ptr bg = item.create_bg(0, 0);
    bn::bg_tiles::set_allow_offset(true);
    return LevelView{ bg, COLS * 8, ROWS * 8 };
}
}
