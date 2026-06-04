#include "game/scene_play.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_items_spronk.h"
#include "bn_sprite_items_enemy.h"

#include "logic/tilemap.h"
#include "logic/player.h"
#include "logic/dungeon1.h"
#include "logic/enemy.h"
#include "logic/meters.h"
#include "engine/input.h"
#include "engine/level_view.h"
#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/hud.h"
#include "engine/save.h"

namespace game
{
namespace
{
    constexpr int LW = 48;
    constexpr int LH = 20;
    uint8_t level_cells[LW * LH];

    // Gate wall (solid while closed): column 24, full height so it can't be jumped over.
    constexpr int GATE_X = 24;
    constexpr int GATE_Y0 = 1;
    constexpr int GATE_Y1 = 17;

    inline void set_tile(int tx, int ty, uint8_t v){ level_cells[ty * LW + tx] = v; }
    inline logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }

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
        for(int x = 8; x <= 12; ++x) set_tile(x, 15, 1);
        for(int y = GATE_Y0; y <= GATE_Y1; ++y) set_tile(GATE_X, y, 1);
        for(int x = 30; x <= 34; ++x) set_tile(x, 14, 2);
        for(int x = 42; x <= 46; ++x) set_tile(x, 8, 1);
    }
}

void run_play(logic::World& world)
{
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    build_dungeon1();
    const bool start_open = world.spronk1_freed; // continuing with the gate already open
    if(start_open)
        for(int y = GATE_Y0; y <= GATE_Y1; ++y) set_tile(GATE_X, y, 0);

    logic::Tilemap map{ LW, LH, level_cells };
    engine::LevelView level = engine::build_level_view(map);
    if(!start_open)
        for(int y = GATE_Y0; y <= GATE_Y1; ++y) engine::set_level_tile(level, GATE_X, y, 3);

    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    level.bg.set_camera(cam);

    const int hw = level.map_px_w / 2;
    const int hh = level.map_px_h / 2;
    auto wx = [&](int level_px){ return level_px - hw; };
    auto wy = [&](int level_px){ return level_px - hh; };

    const logic::Vec2 spawn_pos { fx(3 * 8), fx(13 * 8) };

    logic::Player player;
    player.body.half_w = fx(8);
    player.body.half_h = fx(16);
    player.body.pos = spawn_pos;

    logic::Meter health{ 100, 100 };
    logic::Meter magic{ 0, 100 };
    int invuln = 0;

    engine::Avatar avatar(player, level.map_px_w, level.map_px_h, cam);
    engine::BoltPool bolts(level.map_px_w, level.map_px_h, cam);
    engine::Hud hud;

    logic::Body cage;
    cage.half_w = fx(8); cage.half_h = fx(12);
    cage.pos = { fx(10 * 8), fx(13 * 8) };
    bn::sprite_ptr spronk = bn::sprite_items::spronk.create_sprite(0, 0);
    spronk.set_camera(cam);
    spronk.set_position(wx(cage.pos.x.to_int() + 8), wy(cage.pos.y.to_int() + 8));
    spronk.set_visible(!start_open); // already rescued? then no caged spronk

    logic::Body exit;
    exit.half_w = fx(16); exit.half_h = fx(16);
    exit.pos = { fx(43 * 8), fx(6 * 8) };

    logic::Enemy enemy;
    enemy.body.half_w = fx(8); enemy.body.half_h = fx(8);
    enemy.body.pos = { fx(16 * 8), fx(15 * 8) };
    enemy.left_bound = fx(14 * 8); enemy.right_bound = fx(22 * 8);
    bn::sprite_ptr enemy_sprite = bn::sprite_items::enemy.create_sprite(0, 0);
    enemy_sprite.set_camera(cam);

    int victory_timer = -1; // >=0 once won; returns to title at 0

    while(true)
    {
        logic::InputFrame in = engine::read_input();

        player.abilities.featherleap = world.featherleap;
        player.update(in, map);
        avatar.sync(player);

        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h };
        bolts.update(in.fire_pressed, muzzle, player.facing, map);

        enemy.update(map);
        if(enemy.alive)
        {
            int ex = enemy.body.pos.x.to_int() + enemy.body.half_w.to_int();
            int ey = enemy.body.pos.y.to_int() + enemy.body.half_h.to_int();
            enemy_sprite.set_position(ex - hw, ey - hh);

            if(bolts.consume_hit(enemy.body))
            {
                enemy.kill();
                magic.heal(25);
                enemy_sprite.set_visible(false);
            }
            else if(invuln == 0 && logic::aabb_overlap(player.body, enemy.body))
            {
                health.damage(20);
                invuln = 45;
            }
        }

        if(invuln > 0)
        {
            --invuln;
            avatar.set_visible((invuln / 4) % 2 == 0);
        }
        else
        {
            avatar.set_visible(true);
        }
        if(health.is_empty())
        {
            player.body.pos = spawn_pos;
            player.body.vel = { fx(0), fx(0) };
            health.cur = health.max;
            invuln = 0;
        }

        bool was_freed = world.spronk1_freed;
        logic::try_free_spronk(player.body, cage, world);
        if(world.spronk1_freed && !was_freed)
        {
            for(int y = GATE_Y0; y <= GATE_Y1; ++y)
            {
                set_tile(GATE_X, y, 0);
                engine::set_level_tile(level, GATE_X, y, 0);
            }
            spronk.set_visible(false);
        }

        if(victory_timer < 0 && logic::aabb_overlap(player.body, exit))
        {
            bn::bg_palettes::set_transparent_color(bn::color(6, 22, 8)); // victory green
            engine::write_world(world);                                  // save progress
            victory_timer = 90;
        }
        if(victory_timer > 0 && --victory_timer == 0) return;            // back to title

        hud.update(health, magic);

        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        cam.set_position(cx - hw, cy - hh);

        bn::core::update();
    }
}
}
