#include "game/scene_dungeon.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_items_spronk.h"
#include "bn_sprite_items_enemy.h"

#include "logic/tilemap.h"
#include "logic/player.h"
#include "logic/dungeon1.h"   // try_free_spronk
#include "logic/enemy.h"
#include "logic/meters.h"
#include "engine/input.h"
#include "engine/level_loader.h"
#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/hud.h"
#include "engine/fade.h"

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
    struct EnemyInst { logic::Enemy e; bn::optional<bn::sprite_ptr> sprite; };
}

DungeonResult run_dungeon(const logic::LevelData& level, logic::World& world)
{
    const int d = world.current_dungeon;
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    engine::LoadedLevel lvl = engine::load_level(level);
    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    lvl.view.bg.set_camera(cam);

    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };

    const logic::Vec2 spawn_pos { fx(level.spawn_tx * 8), fx(level.spawn_ty * 8) };

    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = spawn_pos;

    logic::Meter health{ 100, 100 };
    logic::Meter magic{ 0, 100 };
    int invuln = 0;

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    logic::Body cage;
    bn::optional<bn::sprite_ptr> spronk;
    if(level.has_cage)
    {
        cage.half_w = fx(8); cage.half_h = fx(12);
        cage.pos = { fx(level.cage_tx * 8), fx(level.cage_ty * 8) };
        spronk = bn::sprite_items::spronk.create_sprite(0, 0);
        spronk->set_camera(cam);
        spronk->set_position(wx(cage.pos.x.to_int() + 8), wy(cage.pos.y.to_int() + 8));
        spronk->set_visible(!world.spronk_freed(d)); // already freed on a continued game
    }

    logic::Body exit;
    if(level.has_exit)
    {
        exit.half_w = fx(12); exit.half_h = fx(12);
        exit.pos = { fx(level.exit_tx * 8), fx(level.exit_ty * 8) };
    }

    bn::vector<EnemyInst, 8> enemies;
    for(int i = 0; i < level.enemy_count && i < 8; ++i)
    {
        const logic::EntitySpawn& s = level.enemies[i];
        enemies.push_back(EnemyInst{});
        EnemyInst& inst = enemies.back();
        inst.e.body.half_w = fx(8); inst.e.body.half_h = fx(8);
        inst.e.body.pos = { fx(s.tx * 8), fx(s.ty * 8) };
        inst.e.left_bound = fx(s.param0 * 8); inst.e.right_bound = fx(s.param1 * 8);
        inst.sprite = bn::sprite_items::enemy.create_sprite(0, 0);
        inst.sprite->set_camera(cam);
    }

    // Centre the camera on the player BEFORE fading in, else the fade reveals the level
    // centre (and the enemy) and then snaps to Laurel on the first loop frame.
    // (int locals: to_int() returns int32_t == long on arm-none-eabi, ambiguous as bn::fixed.)
    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    cam.set_position(cx0 - hw, cy0 - hh);
    engine::set_fade(16);   // start black; fades in LIVE over the loop so the player is
    int fade_in_t = 16;     // controllable from frame 0 (no frozen-then-snap on held input)

    DungeonResult result = DungeonResult::Quit;
    while(true)
    {
        if(bn::keypad::select_pressed()) { result = DungeonResult::Quit; break; }

        logic::InputFrame in = engine::read_input();
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.update(in, lvl.map);
        avatar.sync(player);

        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h };
        bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map);

        for(EnemyInst& inst : enemies)
        {
            inst.e.update(lvl.map);
            if(inst.e.alive)
            {
                int ex = inst.e.body.pos.x.to_int() + inst.e.body.half_w.to_int();
                int ey = inst.e.body.pos.y.to_int() + inst.e.body.half_h.to_int();
                inst.sprite->set_position(ex - hw, ey - hh);
                if(bolts.consume_hit(inst.e.body))
                {
                    inst.e.kill(); magic.heal(25); inst.sprite->set_visible(false);
                }
                else if(invuln == 0 && logic::aabb_overlap(player.body, inst.e.body))
                {
                    health.damage(20); invuln = 45;
                }
            }
        }

        if(invuln > 0) { --invuln; avatar.set_visible((invuln / 4) % 2 == 0); }
        else avatar.set_visible(true);
        if(health.is_empty())
        {
            player.body.pos = spawn_pos; player.body.vel = { fx(0), fx(0) };
            health.cur = health.max; invuln = 0;
        }

        if(level.has_cage)
        {
            bool was = world.spronk_freed(d);
            logic::try_free_spronk(player.body, cage, world, d);
            if(world.spronk_freed(d) && !was)
            {
                world.grant(logic::Ability::Featherleap); // reward at free-time (M2: Dungeon 1)
                if(spronk) spronk->set_visible(false);
            }
        }

        bool spronk_ok = !level.has_cage || world.spronk_freed(d);
        if(level.has_exit && spronk_ok && logic::aabb_overlap(player.body, exit))
        {
            result = DungeonResult::Cleared; break;
        }

        hud.update(health, magic);
        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        cam.set_position(cx - hw, cy - hh);
        if(fade_in_t > 0) engine::set_fade(--fade_in_t); // live fade-in ramp
        bn::core::update();
    }

    engine::fade_out(16);
    return result;
}
}
