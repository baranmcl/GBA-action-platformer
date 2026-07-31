#include "game/scene_hub.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_keypad.h"

#include "logic/tilemap.h"
#include "logic/tile_ids.h"
#include "logic/player.h"
#include "logic/gates.h"
#include "logic/spell.h"
#include "logic/meters.h"
#include "logic/collision.h"   // aabb_overlap
#include "logic/world_state.h" // World
#include "logic/player_state.h" // sync_health_cap (heart-container max-HP sync)
#include "engine/bolts.h"
#include "engine/spell_pool.h"
#include "engine/level_loader.h"
#include "engine/level_view.h"  // set_level_tile
#include "engine/avatar.h"
#include "engine/fade.h"
#include "engine/hud.h"
#include "engine/pause.h"        // check_pause (START -> GAME PAUSED; global pause)
#include "game/player_session.h" // PlayerSession, SessionIntent, set_clamped_cam
#include "game/levels/hub.h"

namespace game
{
namespace {
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
}

HubResult run_hub(logic::World& world, logic::PlayerState& ps)
{
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    // Keep the HUD's max-HP correct in the hub too: reflect any collected heart containers
    // (PlayerState defaults to 100/100). Only raise the CAP here; pickups refill to full.
    logic::sync_health_cap(ps, world);

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
        int t = logic::door_enterable(dr.dungeon, world) ? logic::tiles::DOOR_OPEN : logic::tiles::DOOR_LOCKED;
        for(int dy = 0; dy < 4; ++dy)
            for(int dx = 0; dx < 2; ++dx)
                engine::set_level_tile(lvl.view, dr.tx + dx, dr.ty - dy, t);
    }

    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    lvl.view.bg.set_camera(cam);
    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;
    // Clamp the camera to the authored hub bounds so the 240x160 view doesn't show the blank
    // margins of the fixed 64x32 background (the plaza is only 48x18). Same clamp as scene_dungeon.

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
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud; // shows the persistent health/magic in the hub too

    // Spell selection lives in PlayerState so it persists across the hub, dungeon rooms, AND
    // hub<->dungeon. ensure_valid initializes a default without clobbering a carried-in choice.
    ps.spell.ensure_valid(world);

    // Player-loop unit: ability sync, input decode, spell HUD icon, vine VFX, muzzle calc.
    game::PlayerSession session(cam, world, ps, player);
    session.refresh_spell_icon();   // one-time: show the carried-in selection immediately

    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    game::set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, HUB_DATA.w, HUB_DATA.h, cx0, cy0);
    engine::set_fade(16);
    int fade_in_t = 16;

    while(true)
    {
        engine::check_pause();   // START -> freeze + "GAME PAUSED" until START again (global pause)
        // Full ability parity with the dungeon: every earned movement ability is live in the hub.
        session.sync_abilities();

        // Read spell intent + cycle FIRST so the selection is current for the grapple/cast branch.
        SessionIntent intent = session.read_intent();

        // Grapple branch — hub-simplified to ANCHOR-ONLY (no blocks/enemies to pull here).
        if(intent.want_grapple) intent.in.grapple_fire = true;
        bool tried_anchor = intent.want_grapple && intent.in.grapple_fire;

        player.update(intent.in, lvl.map);
        avatar.sync(player);

        // Miss detection: tried an anchor-grapple but nothing latched (no grapple point in range).
        if(tried_anchor && !player.grapple.active())
            session.note_anchor_miss(player.facing);

        // Shot aim (Zelda II style, shared with the boss/dungeon): UP = high, DOWN = low, else medium.
        // Always-available wand (basic B attack), mirrored from play_room -- the hub had a
        // SpellPool but no BoltPool, so B did nothing here.
        bolts.update(intent.in.fire_pressed, session.muzzle(), player.facing, lvl.map);
        spells.update_and_cast(intent.cast_spell, ps.spell, magic, session.muzzle(), player.facing, lvl.map);
        spells.despawn_on_solid(lvl.map);

        session.refresh_spell_icon();   // reflect cycle (L) in the HUD icon

        // Door entry: stand on a door + press Up. Only enterable doors transition.
        if(bn::keypad::up_pressed())
        {
            for(int i = 0; i < HUB_DATA.door_count; ++i)
            {
                const logic::DoorSpawn& dr = HUB_DATA.doors[i];
                if(!logic::door_enterable(dr.dungeon, world)) continue;
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

        session.update_vine_vfx(hw, hh);

        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        game::set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, HUB_DATA.w, HUB_DATA.h, cx, cy);
        hud.update(ps.health, ps.magic, world.lives);
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}
}
