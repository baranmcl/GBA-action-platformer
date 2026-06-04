// MILESTONE: Dungeon 1 core loop — free the spronk -> earn Featherleap + open the
// gate -> reach the double-jump-only exit. Still an integrated play loop in main();
// refactored into the scene framework next.
#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_items_spronk.h"

#include "logic/tilemap.h"
#include "logic/player.h"
#include "logic/world_state.h"
#include "logic/dungeon1.h"
#include "engine/input.h"
#include "engine/level_view.h"
#include "engine/avatar.h"
#include "engine/bolts.h"

namespace
{
    constexpr int LW = 48;
    constexpr int LH = 20;
    uint8_t level_cells[LW * LH];

    // Gate wall (solid while closed): column 24, full height (rows 1..17) so it can't
    // be jumped over — only the spronk rescue opens it.
    constexpr int GATE_X = 24;
    constexpr int GATE_Y0 = 1;
    constexpr int GATE_Y1 = 17;

    inline void set_tile(int tx, int ty, uint8_t v){ level_cells[ty * LW + tx] = v; }

    void build_dungeon1()
    {
        for(int y = 0; y < LH; ++y)
            for(int x = 0; x < LW; ++x)
            {
                uint8_t t = 0;
                if(x == 0 || x == LW - 1 || y == 0 || y == LH - 1) t = 1; // border
                if(y >= LH - 2) t = 1;                                    // floor
                set_tile(x, y, t);
            }
        for(int x = 8; x <= 12; ++x) set_tile(x, 15, 1);          // cage platform
        for(int y = GATE_Y0; y <= GATE_Y1; ++y) set_tile(GATE_X, y, 1); // gate pillar (solid)
        for(int x = 30; x <= 34; ++x) set_tile(x, 14, 2);          // one-way (flavour)
        for(int x = 42; x <= 46; ++x) set_tile(x, 8, 1);           // high exit platform (double-jump only)
    }

    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
}

int main()
{
    bn::core::init();
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    build_dungeon1();
    logic::Tilemap map{ LW, LH, level_cells };

    engine::LevelView level = engine::build_level_view(map);
    for(int y = GATE_Y0; y <= GATE_Y1; ++y) engine::set_level_tile(level, GATE_X, y, 3); // gate visual

    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    level.bg.set_camera(cam);

    const int hw = level.map_px_w / 2;
    const int hh = level.map_px_h / 2;
    auto to_world_x = [&](int level_px){ return level_px - hw; };
    auto to_world_y = [&](int level_px){ return level_px - hh; };

    logic::World world; // fresh: no spronk freed, no Featherleap

    logic::Player player;
    player.body.half_w = fx(8);
    player.body.half_h = fx(16);
    player.body.pos = { fx(3 * 8), fx(13 * 8) };

    engine::Avatar avatar(player, level.map_px_w, level.map_px_h, cam);
    engine::BoltPool bolts(level.map_px_w, level.map_px_h, cam);

    // Caged spronk on the platform around tile (10, 14).
    logic::Body cage;
    cage.half_w = fx(8); cage.half_h = fx(12);
    cage.pos = { fx(10 * 8), fx(13 * 8) };
    bn::sprite_ptr spronk = bn::sprite_items::spronk.create_sprite(0, 0);
    spronk.set_camera(cam);
    spronk.set_position(to_world_x(cage.pos.x.to_int() + 8), to_world_y(cage.pos.y.to_int() + 8));

    // Exit zone on the high platform (reachable only with the double jump).
    logic::Body exit;
    exit.half_w = fx(16); exit.half_h = fx(16);
    exit.pos = { fx(43 * 8), fx(6 * 8) };

    bool won = false;

    while(true)
    {
        logic::InputFrame in = engine::read_input();

        // Featherleap is owned only after the spronk is freed.
        player.abilities.featherleap = world.featherleap;
        player.update(in, map);
        avatar.sync(player);

        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h };
        bolts.update(in.fire_pressed, muzzle, player.facing, map);

        // Spronk rescue: grant Featherleap + open the gate (once).
        bool was_freed = world.spronk1_freed;
        logic::try_free_spronk(player.body, cage, world);
        if(world.spronk1_freed && !was_freed)
        {
            for(int y = GATE_Y0; y <= GATE_Y1; ++y)
            {
                set_tile(GATE_X, y, 0);                 // collision opens
                engine::set_level_tile(level, GATE_X, y, 0); // visual opens
            }
            spronk.set_visible(false);                  // spronk is freed
        }

        if(!won && logic::aabb_overlap(player.body, exit))
        {
            won = true;
            bn::bg_palettes::set_transparent_color(bn::color(6, 22, 8)); // victory green
        }

        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        cam.set_position(cx - hw, cy - hh);

        bn::core::update();
    }
}
