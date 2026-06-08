#include "engine/level_view.h"
#include "bn_size.h"
#include "bn_memory.h"
#include "bn_bg_tiles.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_map_ptr.h"
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
                int kind = map.cells[ty * map.w + tx]; // collision TileKind value
                // collision TileKind -> bg tile index (see gates.h tile map). Identity for 0/1/2;
                // Lava(3)->13, Water(4)->16, IcePlatform(5)->19, Updraft(6)->20, WindLeft(7)->21,
                // WindRight(8)->22. Gates/doors/entities overlaid via set_level_tile.
                tile_index = (kind == 3) ? 13 : (kind == 4) ? 16 : (kind == 5) ? 19
                           : (kind == 6) ? 20 : (kind == 7) ? 21 : (kind == 8) ? 22 : kind;
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

void set_level_tile(LevelView& view, int tx, int ty, int tile_index){
    if(tx < 0 || ty < 0 || tx >= COLS || ty >= ROWS) return;
    int ci = s_map_item.cell_index(tx, ty);
    bn::regular_bg_map_cell_info info(s_cells[ci]);
    info.set_tile_index(tile_index);
    info.set_palette_id(0);
    s_cells[ci] = info.cell();
    bn::regular_bg_map_ptr map_ptr = view.bg.map();
    map_ptr.reload_cells_ref();
}
}
