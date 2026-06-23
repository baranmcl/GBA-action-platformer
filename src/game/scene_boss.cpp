#include "game/scene_boss.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_items_king.h"
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_fire_proj.h"
#include "bn_sprite_items_ice_proj.h"
#include "bn_sprite_items_light_proj.h"
#include "bn_sprite_items_grapple_icon.h"
#include "bn_sprite_items_magic_crystal.h"

#include "logic/boss.h"
#include "logic/tilemap.h"
#include "logic/world_state.h"   // max_health_for, lose_life, refill_lives
#include "logic/player.h"
#include "logic/collision.h"     // aabb_overlap
#include "logic/meters.h"
#include "logic/spell.h"
#include "logic/room_graph.h"    // find_entrance
#include "engine/input.h"
#include "engine/spell_input.h"
#include "engine/level_loader.h" // load_level, set_collision_tile
#include "engine/level_view.h"   // set_level_tile
#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/spell_pool.h"
#include "engine/hud.h"
#include "engine/fade.h"
#include "engine/save.h"         // write_world (persist refilled lives)
#include "game/scene_game_over.h"

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }

    // King position — no level-data spawn type; matches the comment marker in
    // tools/levels/dungeon9_arena.txt (KING_SPAWN_TX/TY). Tile coords.
    // TY=16 (not 7): spell projectiles travel HORIZONTALLY (spell.h vel={2*facing,0}), so the King
    // must sit at the firing platforms' height to be hittable. The central/right firing platforms are
    // at row 19 (player stands ~row 17 atop them); a King body centred ~row 17 is hit by a horizontal
    // Light bolt from the platform. (Per-phase King movement / aerial-vs-ground height is a feel item
    // for QA — phase flavour currently comes from the ATTACK patterns, not King position.) QA-tunable.
    constexpr int KING_SPAWN_TX = 19;
    constexpr int KING_SPAWN_TY = 16;

    logic::Body tile_body(int tx, int ty, int hw, int hh){
        logic::Body b{}; b.half_w = fx(hw); b.half_h = fx(hh);
        b.pos = { fx(tx * 8), fx(ty * 8) }; return b;
    }

    // One placeholder attack hitbox: a moving logic::Body + a reused bolt-sprite VFX.
    // Spawned at the start of AttackStep::Active; cleared at Recovery / while exposed.
    struct AttackInst {
        bool active = false;
        logic::Body body;
        logic::Vec2 vel{};
        bn::optional<bn::sprite_ptr> sprite;
    };
}

BossResult run_boss(const logic::DungeonData& arena, logic::World& world, logic::PlayerState& ps)
{
    logic::SpellState& spell = ps.spell;
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    const logic::LevelData& level = *arena.rooms[arena.start_room];
    engine::LoadedLevel lvl = engine::load_level(level);
    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    lvl.view.bg.set_camera(cam);

    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };

    // ---- player spawn (entrance id 0) ----
    logic::EntranceSpawn ent = logic::find_entrance(level, 0);
    const logic::Vec2 spawn_pos { fx(ent.tx * 8), fx(ent.ty * 8) };
    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = spawn_pos;
    player.facing = ent.facing;

    // Vitals: carried-in references, started at FULL for the fight (sync cap first, as run_dungeon does).
    logic::Meter& health = ps.health;
    logic::Meter& magic  = ps.magic;
    ps.health.max = logic::max_health_for(world);
    health.cur = health.max;
    magic.cur  = magic.max;
    spell.ensure_valid(world);
    int invuln = 0;
    constexpr int RESPAWN_IFRAMES = 60;

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    // ---- King ----
    logic::BossState b; b.reset();
    // King hit-body: ~24x24 centred on the King tile (sprite is 32x32; a slightly tighter body
    // so Light/bolts must actually hit the figure). pos = top-left in px.
    logic::Body king_body;
    king_body.half_w = fx(12); king_body.half_h = fx(12);
    king_body.pos = { fx(KING_SPAWN_TX * 8 - 4), fx(KING_SPAWN_TY * 8 - 4) };
    bn::sprite_ptr king = bn::sprite_items::king.create_sprite(0, 0);
    king.set_camera(cam);
    {
        int kcx = king_body.pos.x.to_int() + king_body.half_w.to_int();
        int kcy = king_body.pos.y.to_int() + king_body.half_h.to_int();
        king.set_position(wx(kcx), wy(kcy));
    }

    // ---- attack hitbox (single placeholder; reused per phase) ----
    AttackInst atk;
    atk.sprite = bn::sprite_items::bolt.create_sprite(0, 0);
    atk.sprite->set_camera(cam);
    atk.sprite->set_visible(false);
    atk.sprite->set_scale(2.0);
    bool atk_spawned_this_active = false;   // edge-detect the Active step to spawn once per cycle

    // ---- magic crystal (M10 pattern: full refill on overlap; reset un-collected each attempt) ----
    bool crystal_collected = false;
    bn::optional<bn::sprite_ptr> crystal_sprite;
    int crystal_tx = 0, crystal_ty = 0;
    if(level.magic_crystal_count > 0){
        const logic::MagicCrystalSpawn& mc = level.magic_crystals[0];
        crystal_tx = mc.tx; crystal_ty = mc.ty;
        crystal_sprite = bn::sprite_items::magic_crystal.create_sprite(0, 0);
        crystal_sprite->set_camera(cam);
        crystal_sprite->set_position(wx(mc.tx * 8 + 8), wy(mc.ty * 8 + 8));
    }
    logic::Body crystal_body = tile_body(crystal_tx, crystal_ty, 6, 8);

    // ---- spell HUD icon (top-right; swap art on selection change only) ----
    bn::sprite_ptr spell_icon = bn::sprite_items::fire_proj.create_sprite(104, -68);
    logic::SpellId last_icon = logic::SpellId::None;
    auto refresh_spell_icon = [&]{
        if(spell.selected != last_icon){
            if(spell.selected == logic::SpellId::Ice)          spell_icon.set_item(bn::sprite_items::ice_proj);
            else if(spell.selected == logic::SpellId::Fire)    spell_icon.set_item(bn::sprite_items::fire_proj);
            else if(spell.selected == logic::SpellId::Grapple) spell_icon.set_item(bn::sprite_items::grapple_icon);
            else if(spell.selected == logic::SpellId::Light)   spell_icon.set_item(bn::sprite_items::light_proj);
            last_icon = spell.selected;
        }
        spell_icon.set_visible(spell.selected != logic::SpellId::None);
    };
    refresh_spell_icon();

    // FIXED camera at the arena centre (do NOT scroll-follow).
    const int arena_cx = level.w * 8 / 2;
    const int arena_cy = level.h * 8 / 2;
    cam.set_position(arena_cx - hw, arena_cy - hh);

    // restart_fight: every restart path (death-with-lives, Game-Over Continue) runs this.
    auto restart_fight = [&]{
        b.on_player_death();                       // BossState -> P1, full HP, timers cleared
        health.cur = health.max; magic.cur = magic.max;
        player.body.pos = spawn_pos; player.body.vel = { fx(0), fx(0) };
        player.body.on_ground = false;
        player.dash = logic::DashState{};
        player.grapple = logic::GrappleState{};
        player.stone = logic::StoneState{};
        player.facing = ent.facing;
        avatar.sync(player);
        invuln = RESPAWN_IFRAMES;
        atk.active = false; atk_spawned_this_active = false;
        if(atk.sprite) atk.sprite->set_visible(false);
        crystal_collected = false;
        if(crystal_sprite) crystal_sprite->set_visible(true);
    };

    engine::set_fade(16);
    int fade_in_t = 16;

    while(true)
    {
        // ---- input + abilities (no quit/restart keys: run_boss has no mid-fight exit) ----
        logic::InputFrame in = engine::read_input();
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.abilities.glide       = world.has(logic::Ability::Glide);
        player.abilities.dash        = world.has(logic::Ability::Dash);
        player.abilities.grapple     = world.has(logic::Ability::Grapple);
        player.abilities.stone       = world.has(logic::Ability::Stone);

        engine::SpellIntent si = engine::read_spell_intent();
        if(si.cycle) spell.cycle(world);
        bool want_grapple = si.cast && spell.selected == logic::SpellId::Grapple;
        in.grapple_fire = want_grapple;   // arena has anchors but no pull-blocks/enemies to grab
        bool cast_spell = si.cast && (spell.selected == logic::SpellId::Fire ||
                                      spell.selected == logic::SpellId::Ice  ||
                                      spell.selected == logic::SpellId::Light);

        player.update(in, lvl.map);
        avatar.sync(player);

        // ---- boss logic tick ----
        b.tick();

        // ---- King render: telegraph tint while Telegraph, distinct flash while EXPOSED ----
        {
            int kcx = king_body.pos.x.to_int() + king_body.half_w.to_int();
            int kcy = king_body.pos.y.to_int() + king_body.half_h.to_int();
            king.set_position(wx(kcx), wy(kcy));
            if(b.exposed()){
                // EXPOSED = visibly distinct: rapid blink (vulnerable / stunned).
                king.set_visible((b.expose_timer / 4) % 2 == 0);
            } else if(b.current_step() == logic::AttackStep::Telegraph){
                // Telegraph: slow pulse so the wind-up reads as a warning.
                king.set_visible((b.attack_timer / 8) % 2 == 0);
            } else {
                king.set_visible(true);
            }
        }

        // ---- player position helpers ----
        int pcx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int pcy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        int kcx = king_body.pos.x.to_int() + king_body.half_w.to_int();
        int kcy = king_body.pos.y.to_int() + king_body.half_h.to_int();

        // ---- attack spawn/playback — ONLY while NOT exposed (expose = clean window) ----
        if(b.exposed()){
            // Stunned: freeze + hide any active attack; no contact damage during the window.
            atk.active = false; atk_spawned_this_active = false;
            if(atk.sprite) atk.sprite->set_visible(false);
        } else {
            logic::AttackStep step = b.current_step();
            // Resolve the underlying combat phase (Exposed never reaches here).
            logic::BossPhase ph = b.phase;
            if(step == logic::AttackStep::Active){
                if(!atk_spawned_this_active){
                    // Spawn the phase-appropriate placeholder attack.
                    atk.active = true;
                    atk_spawned_this_active = true;
                    if(ph == logic::BossPhase::P1){
                        // P1 aerial sweep: a horizontal projectile crossing at the player's height.
                        atk.body.half_w = fx(6); atk.body.half_h = fx(6);
                        int sweep_y = pcy;   // aim at the player's current height
                        int from_left = (pcx >= kcx) ? -1 : 1;  // come from the side toward the player
                        int start_x = (from_left == 1) ? 16 : (level.w * 8 - 16);
                        atk.body.pos = { fx(start_x - atk.body.half_w.to_int()),
                                         fx(sweep_y - atk.body.half_h.to_int()) };
                        atk.vel = { fx(4 * from_left), fx(0) };
                    } else if(ph == logic::BossPhase::P2){
                        // P2 ground slam: a shockwave body at floor level expanding outward from the King.
                        atk.body.half_w = fx(8); atk.body.half_h = fx(8);
                        int floor_y = level.h * 8 - 24;   // just above the solid floor rows 30-31
                        atk.body.pos = { fx(kcx - atk.body.half_w.to_int()),
                                         fx(floor_y - atk.body.half_h.to_int()) };
                        atk.vel = { fx((pcx >= kcx) ? 5 : -5), fx(0) };  // travel along the floor toward the player
                    } else {
                        // P3 spread/fan: a descending projectile from the King toward the player.
                        atk.body.half_w = fx(6); atk.body.half_h = fx(6);
                        atk.body.pos = { fx(kcx - atk.body.half_w.to_int()),
                                         fx(kcy - atk.body.half_h.to_int()) };
                        int dx = (pcx >= kcx) ? 3 : -3;
                        atk.vel = { fx(dx), fx(4) };   // fan downward toward the player
                    }
                }
            } else if(step == logic::AttackStep::Recovery){
                atk.active = false;
                atk_spawned_this_active = false;
                if(atk.sprite) atk.sprite->set_visible(false);
            } else { // Telegraph
                atk_spawned_this_active = false;
            }
        }

        // ---- attack advance + render + contact ----
        if(atk.active){
            atk.body.pos.x = atk.body.pos.x + atk.vel.x;
            atk.body.pos.y = atk.body.pos.y + atk.vel.y;
            int ax = atk.body.pos.x.to_int() + atk.body.half_w.to_int();
            int ay = atk.body.pos.y.to_int() + atk.body.half_h.to_int();
            // Expire off-arena.
            if(ax < 0 || ax > level.w * 8 || ay < 0 || ay > level.h * 8){
                atk.active = false;
                if(atk.sprite) atk.sprite->set_visible(false);
            } else {
                if(atk.sprite){ atk.sprite->set_position(wx(ax), wy(ay)); atk.sprite->set_visible(true); }
                if(invuln == 0 && !player.dash.invincible() && logic::aabb_overlap(player.body, atk.body)){
                    health.damage(20); invuln = 45;
                }
            }
        }

        // ---- projectile pools ----
        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h };
        bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map);
        spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map);

        // ---- damage resolution (mirrors the dungeon's consume_hit hooks; King is a logic::Body) ----
        // Light ALWAYS exposes/refreshes — runs every frame regardless of phase (M10 Light-clears
        // hook, repointed to expose). on_light_hit() is a no-op once defeated.
        if(spells.consume_hit(king_body, logic::SpellId::Light)) b.on_light_hit();
        // Wounding lands ONLY while EXPOSED (the King is immune while shielded). on_wound() itself
        // also guards on exposed(), but we gate here so a shot never silently vanishes off the King
        // mid-shield. An ELEMENTAL wound also refills magic (sustains casts; dungeon kill-refill feel).
        if(b.exposed()){
            if(bolts.consume_hit(king_body)){
                b.on_wound(logic::WOUND_DMG);
            } else if(spells.consume_hit(king_body, logic::SpellId::Fire)
                   || spells.consume_hit(king_body, logic::SpellId::Ice)){
                b.on_wound(logic::WOUND_DMG);
                magic.heal(25);
            }
        }
        if(b.defeated()) return BossResult::Victory;

        spells.despawn_on_solid(lvl.map);

        // ---- magic crystal: full refill on overlap (reset un-collected each attempt) ----
        if(!crystal_collected && logic::aabb_overlap(player.body, crystal_body)){
            magic.cur = magic.max;
            crystal_collected = true;
            if(crystal_sprite) crystal_sprite->set_visible(false);
        }

        // ---- i-frames blink ----
        if(invuln > 0){ --invuln; avatar.set_visible((invuln / 4) % 2 == 0); }
        else avatar.set_visible(true);

        // ---- death / lives / Game-Over (M9 flow; the WHOLE fight restarts, not the approach) ----
        if(health.is_empty()){
            logic::lose_life(world);
            engine::write_world(world);   // persist the decremented count immediately
            if(world.lives > 0){
                restart_fight();          // full-fight restart: BossState->P1, vitals, position, crystal
            } else {
                game::GameOverChoice c = game::run_game_over(world);
                logic::refill_lives(world);   // both choices refill to max
                engine::write_world(world);   // persist the refill — the save never holds lives==0
                if(c == game::GameOverChoice::QuitToTitle) return BossResult::QuitToTitle;
                restart_fight();              // Continue: restart the fight (vitals refilled inside)
            }
            // run_game_over took over the screen; re-arm the fade-in so the fight fades back in.
            engine::set_fade(16); fade_in_t = 16;
        }

        refresh_spell_icon();
        hud.update(health, magic, world.lives);
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}
}
