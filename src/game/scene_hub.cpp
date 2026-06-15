#include "game/scene_hub.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"
#include "bn_keypad.h"
#include "bn_sprite_items_fire_proj.h"
#include "bn_sprite_items_ice_proj.h"
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_grapple_icon.h"

#include "logic/tilemap.h"
#include "logic/player.h"
#include "logic/gates.h"
#include "logic/spell.h"
#include "logic/meters.h"
#include "logic/collision.h"   // aabb_overlap
#include "logic/world_state.h" // max_health_for (heart-container max-HP sync)
#include "engine/input.h"
#include "engine/spell_input.h"   // read_spell_intent, SpellIntent
#include "engine/spell_pool.h"
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
    // Door N enterable: D1 always; each next door opens once the prior dungeon's spronk is freed.
    bool door_enterable(int n, const logic::World& w){
        return n == 1
            || (n == 2 && w.spronk_freed(1))
            || (n == 3 && w.spronk_freed(2))
            || (n == 4 && w.spronk_freed(3))
            || (n == 5 && w.spronk_freed(4))
            || (n == 6 && w.spronk_freed(5))
            || (n == 7 && w.spronk_freed(6))
            || (n == 8 && w.spronk_freed(7));
    }
}

HubResult run_hub(logic::World& world, logic::PlayerState& ps)
{
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    // Keep the HUD's max-HP correct in the hub too: reflect any collected heart containers
    // (PlayerState defaults to 100/100). Only raise the CAP here; pickups refill to full.
    ps.health.max = logic::max_health_for(world);
    if(ps.health.cur > ps.health.max) ps.health.cur = ps.health.max;

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
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };
    // Clamp the camera to the authored hub bounds so the 240x160 view doesn't show the blank
    // margins of the fixed 64x32 background (the plaza is only 48x18). Same clamp as scene_dungeon.
    auto set_clamped_cam = [&](int cx, int cy){
        const int ll = -hw, lt = -hh, lr = ll + HUB_DATA.w * 8, lb = lt + HUB_DATA.h * 8;
        const int minx = ll + 120, maxx = lr - 120, miny = lt + 80, maxy = lb - 80;
        int camx = cx - hw, camy = cy - hh;
        camx = (minx <= maxx) ? (camx < minx ? minx : camx > maxx ? maxx : camx) : (ll + lr) / 2;
        camy = (miny <= maxy) ? (camy < miny ? miny : camy > maxy ? maxy : camy) : (lt + lb) / 2;
        cam.set_position(camx, camy);
    };

    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    // Spawn at the door of the dungeon we just came from (emerge where we entered), facing into the
    // plaza. last_dungeon == 0 means first entry from the title screen -> use the default spawn.
    // The door's base sits on the floor at dr.ty; mirror the default spawn's vertical offset
    // (HUB spawn_ty is 3 tiles above the door base) so the player lands cleanly on the floor in
    // front of the archway. Door entry needs a fresh Up press, so spawning here can't re-enter it.
    int spawn_tx = HUB_DATA.spawn_tx;
    int spawn_ty = HUB_DATA.spawn_ty;
    if(ps.last_dungeon > 0)
    {
        for(int i = 0; i < HUB_DATA.door_count; ++i)
        {
            const logic::DoorSpawn& dr = HUB_DATA.doors[i];
            if(dr.dungeon == ps.last_dungeon)
            {
                spawn_tx = dr.tx;
                spawn_ty = dr.ty - 3;                   // stand on the floor under/in front of the archway
                player.facing = (dr.tx < HUB_DATA.w / 2) ? 1 : -1; // face inward toward the plaza centre
                break;
            }
        }
    }
    player.body.pos = { fx(spawn_tx * 8), fx(spawn_ty * 8) };

    logic::Meter& magic = ps.magic;   // earned-magic pool (banked across hub <-> dungeon)

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud; // shows the persistent health/magic in the hub too

    // Spell selection lives in PlayerState so it persists across the hub, dungeon rooms, AND
    // hub<->dungeon. ensure_valid initializes a default without clobbering a carried-in choice.
    ps.spell.ensure_valid(world);
    logic::SpellState& spell = ps.spell;

    // Vine VFX: 4 dot sprites (bolt reused as placeholder) drawn along player->anchor line.
    // Visible only while player.grapple.active() (or during a miss-vine animation).
    constexpr int VINE_SEGS = 4;
    bn::vector<bn::sprite_ptr, VINE_SEGS> vine_segs;
    for(int i = 0; i < VINE_SEGS; ++i){
        vine_segs.push_back(bn::sprite_items::bolt.create_sprite(0, 0));
        vine_segs.back().set_camera(cam);
        vine_segs.back().set_visible(false);
        vine_segs.back().set_scale(0.5);
    }

    // Spell HUD icon — screen-fixed top-RIGHT. Shows the SELECTED tool's art (fire/ice/grapple).
    bn::sprite_ptr spell_icon = bn::sprite_items::fire_proj.create_sprite(104, -68);
    logic::SpellId last_icon = logic::SpellId::None;
    auto refresh_spell_icon = [&]{
        if(spell.selected != last_icon){
            if(spell.selected == logic::SpellId::Ice)          spell_icon.set_item(bn::sprite_items::ice_proj);
            else if(spell.selected == logic::SpellId::Fire)    spell_icon.set_item(bn::sprite_items::fire_proj);
            else if(spell.selected == logic::SpellId::Grapple) spell_icon.set_item(bn::sprite_items::grapple_icon);
            last_icon = spell.selected;
        }
        spell_icon.set_visible(spell.selected != logic::SpellId::None);
    };
    refresh_spell_icon();

    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    set_clamped_cam(cx0, cy0);
    engine::set_fade(16);
    int fade_in_t = 16;
    int miss_vine_t   = 0;  // counts down from 10; >0 => miss-vine animation active
    int miss_vine_dir = 1;  // facing direction when the miss was fired

    while(true)
    {
        logic::InputFrame in = engine::read_input();
        // Full ability parity with the dungeon: every earned movement ability is live in the hub.
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.abilities.glide       = world.has(logic::Ability::Glide);
        player.abilities.dash        = world.has(logic::Ability::Dash);
        player.abilities.grapple     = world.has(logic::Ability::Grapple);
        player.abilities.stone       = world.has(logic::Ability::Stone);  // harmless on flat ground; kit parity

        // Read spell intent + cycle FIRST so the selection is current for the grapple/cast branch.
        engine::SpellIntent si = engine::read_spell_intent();
        if(si.cycle) spell.cycle(world);

        // Grapple branch — hub-simplified to ANCHOR-ONLY (no blocks/enemies to pull here).
        bool want_grapple = si.cast && spell.selected == logic::SpellId::Grapple;
        in.grapple_fire = false;
        if(want_grapple) in.grapple_fire = true;
        bool tried_anchor = want_grapple && in.grapple_fire;

        bool cast_spell = si.cast && (spell.selected == logic::SpellId::Fire ||
                                      spell.selected == logic::SpellId::Ice);

        player.update(in, lvl.map);
        avatar.sync(player);

        // Miss detection: tried an anchor-grapple but nothing latched (no grapple point in range).
        if(tried_anchor && !player.grapple.active()){
            miss_vine_t   = 10;
            miss_vine_dir = player.facing;
        }

        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h };
        spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map);
        spells.despawn_on_solid(lvl.map);

        refresh_spell_icon();   // reflect cycle (L) in the HUD icon

        // Door entry: stand on a door + press Up. Only enterable doors transition.
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

        // ---- vine VFX: draw dot segments along player->anchor while grapple active ----
        if(miss_vine_t > 0) --miss_vine_t;
        if(player.grapple.active()){
            int px_ = player.body.pos.x.to_int() + player.body.half_w.to_int();
            int py_ = player.body.pos.y.to_int() + player.body.half_h.to_int();
            int ax_ = player.grapple.anchor_tx * 8 + 4;
            int ay_ = player.grapple.anchor_ty * 8 + 4;
            for(int i = 0; i < VINE_SEGS; ++i){
                int t = i + 1;  // t in 1..VINE_SEGS (skip the player pos itself)
                int sx_ = px_ + (ax_ - px_) * t / (VINE_SEGS + 1);
                int sy_ = py_ + (ay_ - py_) * t / (VINE_SEGS + 1);
                vine_segs[i].set_position(wx(sx_), wy(sy_));
                vine_segs[i].set_visible(true);
            }
        } else if(miss_vine_t > 0){
            // Miss: shoot vine out and retract. Duration 10 frames; peaks at frame 5.
            int elapsed = 10 - miss_vine_t;           // 0..9 as the timer runs from 10 down to 1
            int half = 5;
            int reach_tiles;
            if(elapsed < half){
                reach_tiles = (logic::GrappleState::RANGE * (elapsed + 1)) / half; // 0->RANGE
            } else {
                reach_tiles = (logic::GrappleState::RANGE * (10 - elapsed)) / half; // RANGE->0
            }
            if(reach_tiles < 1) reach_tiles = 1;
            int px_ = player.body.pos.x.to_int() + player.body.half_w.to_int();
            int py_ = player.body.pos.y.to_int() + player.body.half_h.to_int();
            int reach_px = reach_tiles * 8;
            for(int i = 0; i < VINE_SEGS; ++i){
                int t = i + 1;
                int sx_ = px_ + (miss_vine_dir * reach_px * t) / (VINE_SEGS + 1);
                int sy_ = py_;  // horizontal shot (stays at player centre height)
                vine_segs[i].set_position(wx(sx_), wy(sy_));
                vine_segs[i].set_visible(true);
            }
        } else {
            for(int i = 0; i < VINE_SEGS; ++i) vine_segs[i].set_visible(false);
        }

        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        set_clamped_cam(cx, cy);
        hud.update(ps.health, ps.magic, world.lives);
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}
}
