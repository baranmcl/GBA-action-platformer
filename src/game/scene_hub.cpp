#include "game/scene_hub.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_keypad.h"

#include "logic/tilemap.h"
#include "logic/player.h"
#include "logic/gates.h"
#include "logic/collision.h"   // aabb_overlap
#include "engine/input.h"
#include "engine/level_loader.h"
#include "engine/level_view.h"  // set_level_tile
#include "engine/avatar.h"
#include "engine/fade.h"
#include "engine/hud.h"
#include "game/levels/hub.h"

namespace game
{
namespace {
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
    // Door N enterable: D1 always; D2 once D1's spronk is freed; D3-8 not built yet.
    bool door_enterable(int n, const logic::World& w){
        return n == 1 || (n == 2 && w.spronk_freed(1));
    }
}

HubResult run_hub(logic::World& world, logic::PlayerState& ps)
{
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    engine::LoadedLevel lvl = engine::load_level(HUB_DATA);

    // Render gate state from the save: a passable gate opens; otherwise it's a solid wall.
    for(int i = 0; i < HUB_DATA.gate_count; ++i)
    {
        const logic::GateSpawn& g = HUB_DATA.gates[i];
        bool open = logic::can_pass(g.type, world.abilities);
        engine::set_collision_tile(g.tx, g.ty, open ? 0 : 1);
        engine::set_level_tile(lvl.view, g.tx, g.ty, open ? 0 : logic::gate_info(g.type).bg_tile);
    }
    // Render doors as a 2-wide x 4-tall archway (Laurel is 16x32). Dungeon 1 open, others locked.
    for(int i = 0; i < HUB_DATA.door_count; ++i)
    {
        const logic::DoorSpawn& dr = HUB_DATA.doors[i];
        int t = door_enterable(dr.dungeon, world) ? 5 : 6; // open vs locked
        for(int dy = 0; dy < 4; ++dy)
            for(int dx = 0; dx < 2; ++dx)
                engine::set_level_tile(lvl.view, dr.tx + dx, dr.ty - dy, t);
    }

    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    lvl.view.bg.set_camera(cam);
    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;

    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = { fx(HUB_DATA.spawn_tx * 8), fx(HUB_DATA.spawn_ty * 8) };

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud; // shows the persistent health/magic in the hub too

    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    cam.set_position(cx0 - hw, cy0 - hh);
    engine::set_fade(16);
    int fade_in_t = 16;

    while(true)
    {
        logic::InputFrame in = engine::read_input();
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.update(in, lvl.map);
        avatar.sync(player);

        // Door entry: stand on a door + press Up. Only Dungeon 1 is enterable in M2.
        if(bn::keypad::up_pressed())
        {
            for(int i = 0; i < HUB_DATA.door_count; ++i)
            {
                const logic::DoorSpawn& dr = HUB_DATA.doors[i];
                if(!door_enterable(dr.dungeon, world)) continue;
                logic::Body door; // matches the 2x4 archway region
                door.half_w = fx(8); door.half_h = fx(16);
                door.pos = { fx(dr.tx * 8), fx((dr.ty - 3) * 8) };
                if(logic::aabb_overlap(player.body, door))
                {
                    engine::fade_out(16);
                    return HubResult{ dr.dungeon };
                }
            }
        }

        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        cam.set_position(cx - hw, cy - hh);
        hud.update(ps.health, ps.magic);
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}
}
