// MILESTONE: "Laurel moves on a tilemap" (Phase 2/3 integration test).
// Hardcoded test level + Phase-1 physics + input adapter + avatar + camera.
// Refactored into the proper scene framework in the next Phase 3 step.
#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"

#include "logic/tilemap.h"
#include "logic/player.h"
#include "engine/input.h"
#include "engine/level_view.h"
#include "engine/avatar.h"
#include "engine/bolts.h"

namespace
{
    constexpr int LW = 40;
    constexpr int LH = 20;
    uint8_t level_cells[LW * LH];

    void build_test_level()
    {
        for(int y = 0; y < LH; ++y)
        {
            for(int x = 0; x < LW; ++x)
            {
                uint8_t t = 0;
                if(x == 0 || x == LW - 1 || y == 0 || y == LH - 1) t = 1; // solid border
                if(y >= LH - 2) t = 1;                                    // 2-row floor
                level_cells[y * LW + x] = t;
            }
        }
        for(int x = 8; x < 14; ++x)   level_cells[(LH - 6) * LW + x] = 2;  // one-way platform
        for(int y = LH - 5; y < LH - 1; ++y) level_cells[y * LW + 22] = 1; // a wall to bump
        for(int x = 26; x < 30; ++x)  level_cells[(LH - 4) * LW + x] = 1;  // a high ledge
    }
}

int main()
{
    bn::core::init();
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    build_test_level();
    logic::Tilemap map{ LW, LH, level_cells };

    engine::LevelView level = engine::build_level_view(map);
    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    level.bg.set_camera(cam);

    logic::Player player;
    player.body.half_w = logic::Fixed::from_int(8);   // 16 wide
    player.body.half_h = logic::Fixed::from_int(16);  // 32 tall
    player.body.pos = { logic::Fixed::from_int(3 * 8), logic::Fixed::from_int(10 * 8) };
    player.abilities.featherleap = true;              // enable double jump for testing

    engine::Avatar avatar(player, level.map_px_w, level.map_px_h, cam);
    engine::BoltPool bolts(level.map_px_w, level.map_px_h, cam);

    const int hw = level.map_px_w / 2;
    const int hh = level.map_px_h / 2;

    while(true)
    {
        logic::InputFrame in = engine::read_input();
        player.update(in, map);
        avatar.sync(player);

        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h };
        bolts.update(in.fire_pressed, muzzle, player.facing, map);

        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        cam.set_position(cx - hw, cy - hh);

        bn::core::update();
    }
}
