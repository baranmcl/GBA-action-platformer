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
#include "bn_sprite_items_fire_enemy.h"
#include "bn_sprite_items_block.h"
#include "bn_sprite_items_shrine.h"
#include "bn_sprite_items_fire_proj.h"

#include "logic/tilemap.h"
#include "logic/player.h"
#include "logic/dungeon1.h"      // try_free_spronk
#include "logic/enemy.h"
#include "logic/meters.h"
#include "logic/spell.h"
#include "logic/hazard.h"
#include "logic/fire_effect.h"
#include "logic/pushable_block.h"
#include "logic/puzzle.h"
#include "logic/gates.h"
#include "engine/input.h"
#include "engine/spell_input.h"
#include "engine/level_loader.h"  // load_level, set_collision_tile
#include "engine/level_view.h"    // set_level_tile
#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/fire_pool.h"
#include "engine/hud.h"
#include "engine/fade.h"

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
    int px2t(logic::Fixed p){ return logic::Tilemap::px_to_tile(p); }

    struct EnemyInst { logic::Enemy e; bn::optional<bn::sprite_ptr> sprite; };
    struct GateInst  { logic::GateSpawn spawn; logic::Body body; bool open = false; };
    struct BlockInst { logic::PushableBlock blk; bn::optional<bn::sprite_ptr> sprite; };
    struct BrazierInst { int tx, ty, group; logic::Body body; bool lit = false; };
    struct ShrineInst { logic::AbilityPickup pk; logic::Body body; bn::optional<bn::sprite_ptr> sprite; };
    // src_tx/ty: plate or button tile to test; group: brazier group (Braziers kind)
    struct TriggerInst { logic::Trigger trig; int src_tx, src_ty, group; bool applied = false; };

    logic::Body tile_body(int tx, int ty, int hw, int hh){
        logic::Body b{}; b.half_w = fx(hw); b.half_h = fx(hh);
        b.pos = { fx(tx * 8), fx(ty * 8) }; return b;
    }
    // A door/gate is a player-sized 2-wide x 4-tall region anchored top-left at (tx,ty).
    void fill_region(engine::LevelView& view, int tx, int ty, int bg){
        for(int dy = 0; dy < 4; ++dy) for(int dx = 0; dx < 2; ++dx){
            engine::set_collision_tile(tx + dx, ty + dy, 1);
            engine::set_level_tile(view, tx + dx, ty + dy, bg);
        }
    }
    void open_region(engine::LevelView& view, int tx, int ty){
        for(int dy = 0; dy < 4; ++dy) for(int dx = 0; dx < 2; ++dx){
            engine::set_collision_tile(tx + dx, ty + dy, 0);
            engine::set_level_tile(view, tx + dx, ty + dy, 0);
        }
    }
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

    logic::SpellState spell; spell.refresh(world);

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::FirePool fire(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    // Spell HUD icon (fire) — screen-fixed, shown once Fire is owned.
    bn::sprite_ptr spell_icon = bn::sprite_items::fire_proj.create_sprite(-100, -72);
    spell_icon.set_visible(spell.selected == logic::SpellId::Fire);

    // ---- cage / spronk ----
    logic::Body cage;
    bn::optional<bn::sprite_ptr> spronk;
    if(level.has_cage){
        cage = tile_body(level.cage_tx, level.cage_ty, 8, 12);
        spronk = bn::sprite_items::spronk.create_sprite(0, 0);
        spronk->set_camera(cam);
        spronk->set_position(wx(cage.pos.x.to_int() + 8), wy(cage.pos.y.to_int() + 8));
        spronk->set_visible(!world.spronk_freed(d));
    }
    logic::Body exit;
    if(level.has_exit) exit = tile_body(level.exit_tx, level.exit_ty, 12, 12);

    // ---- enemies (fire_immune from param2 bit0) ----
    bn::vector<EnemyInst, 8> enemies;
    for(int i = 0; i < level.enemy_count && i < 8; ++i){
        const logic::EntitySpawn& s = level.enemies[i];
        enemies.push_back(EnemyInst{});
        EnemyInst& inst = enemies.back();
        inst.e.body.half_w = fx(8); inst.e.body.half_h = fx(8);
        inst.e.body.pos = { fx(s.tx * 8), fx(s.ty * 8) };
        inst.e.left_bound = fx(s.param0 * 8); inst.e.right_bound = fx(s.param1 * 8);
        inst.e.fire_immune = (s.param2 & 1) != 0;
        inst.sprite = (inst.e.fire_immune ? bn::sprite_items::fire_enemy.create_sprite(0, 0)
                                          : bn::sprite_items::enemy.create_sprite(0, 0));
        inst.sprite->set_camera(cam);
    }

    // ---- gates (vine/ice obstacle; gap geometry) ----
    bn::vector<GateInst, 24> gates;
    for(int i = 0; i < level.gate_count && i < 24; ++i){
        const logic::GateSpawn& g = level.gates[i];
        gates.push_back(GateInst{ g, tile_body(g.tx, g.ty, 8, 16), false }); // 2x4 door region
        GateInst& gi = gates.back();
        const logic::GateInfo& info = logic::gate_info(g.type);
        bool passable = info.is_geometry && logic::can_pass(g.type, world.abilities);
        if(passable){ gi.open = true; }                          // geometry gate already owned -> open
        else { fill_region(lvl.view, g.tx, g.ty, info.bg_tile); } // closed -> 2x4 solid + visible
    }

    // ---- ability shrines ----
    bn::vector<ShrineInst, 4> shrines;
    for(int i = 0; i < level.pickup_count && i < 4; ++i){
        const logic::AbilityPickup& p = level.pickups[i];
        shrines.push_back(ShrineInst{ p, tile_body(p.tx, p.ty, 6, 8), {} });
        ShrineInst& si = shrines.back();
        si.sprite = bn::sprite_items::shrine.create_sprite(0, 0);
        si.sprite->set_camera(cam);
        si.sprite->set_position(wx(p.tx * 8 + 8), wy(p.ty * 8 + 8));
        si.sprite->set_visible(!world.has(p.ability));   // already taken on a continued game
    }

    // ---- pushable blocks (solid collision cell + 8x8 sprite) ----
    bn::vector<BlockInst, 8> blocks;
    for(int i = 0; i < level.block_count && i < 8; ++i){
        const logic::BlockSpawn& b = level.blocks[i];
        blocks.push_back(BlockInst{ logic::PushableBlock{ b.tx, b.ty }, {} });
        BlockInst& bi = blocks.back();
        engine::set_collision_tile(b.tx, b.ty, 1);       // block is solid; bg stays blank, sprite shows it
        bi.sprite = bn::sprite_items::block.create_sprite(0, 0);
        bi.sprite->set_camera(cam);
    }

    // ---- braziers (bg tile 14 unlit; Body for fire-hit) ----
    bn::vector<BrazierInst, 16> braziers;
    for(int i = 0; i < level.brazier_count && i < 16; ++i){
        const logic::BrazierSpawn& b = level.braziers[i];
        braziers.push_back(BrazierInst{ b.tx, b.ty, b.group, tile_body(b.tx, b.ty, 4, 4), false });
        engine::set_level_tile(lvl.view, b.tx, b.ty, 14);
    }

    // ---- plates (tile 17) / buttons (tile 18) + triggers ----
    bn::vector<TriggerInst, 16> triggers;
    for(int i = 0; i < level.plate_count && i < 16; ++i){
        const logic::PlateSpawn& p = level.plates[i];
        engine::set_level_tile(lvl.view, p.tx, p.ty, 17);
        logic::Trigger t = logic::Trigger::plate(); t.target_tx = p.target_tx; t.target_ty = p.target_ty;
        triggers.push_back(TriggerInst{ t, p.tx, p.ty, -1, false });
    }
    for(int i = 0; i < level.button_count && i < 16; ++i){
        const logic::ButtonSpawn& b = level.buttons[i];
        engine::set_level_tile(lvl.view, b.tx, b.ty, 18);
        logic::Trigger t = logic::Trigger::button(); t.target_tx = b.target_tx; t.target_ty = b.target_ty;
        triggers.push_back(TriggerInst{ t, b.tx, b.ty, -1, false });
    }
    for(int g = 0; g < level.brazier_group_count && g < 16; ++g){
        const logic::BrazierGroupSpawn& bg = level.brazier_groups[g];
        logic::Trigger t = logic::Trigger::braziers(bg.total); t.target_tx = bg.target_tx; t.target_ty = bg.target_ty;
        triggers.push_back(TriggerInst{ t, 0, 0, g, false });
    }

    // Centre camera on the player before fading in (avoids a snap on frame 0).
    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    cam.set_position(cx0 - hw, cy0 - hh);
    engine::set_fade(16);
    int fade_in_t = 16;
    int push_cd = 0;

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

        engine::SpellIntent si = engine::read_spell_intent();
        if(si.cycle) spell.cycle(world);
        fire.update_and_cast(si.cast, spell, magic, muzzle, player.facing, lvl.map);

        // ---- fire resolution (ORDER: gates -> braziers -> enemies -> despawn-on-solid) ----
        for(GateInst& gi : gates){
            if(!gi.open && logic::fire_clears_gate(gi.spawn.type) && fire.consume_hit(gi.body)){
                gi.open = true;
                open_region(lvl.view, gi.spawn.tx, gi.spawn.ty);
            }
        }
        for(BrazierInst& bi : braziers){
            if(!bi.lit && fire.consume_hit(bi.body)){
                bi.lit = true;
                engine::set_level_tile(lvl.view, bi.tx, bi.ty, 15);
            }
        }

        // ---- enemies: patrol, render, bolt-kill(+magic), fire-kill(no magic unless immune), contact ----
        for(EnemyInst& inst : enemies){
            inst.e.update(lvl.map);
            if(!inst.e.alive) continue;
            int ex = inst.e.body.pos.x.to_int() + inst.e.body.half_w.to_int();
            int ey = inst.e.body.pos.y.to_int() + inst.e.body.half_h.to_int();
            inst.sprite->set_position(ex - hw, ey - hh);
            if(bolts.consume_hit(inst.e.body)){
                inst.e.kill(); magic.heal(25); inst.sprite->set_visible(false);
            } else if(fire.consume_hit(inst.e.body)){
                if(!inst.e.fire_immune){ inst.e.kill(); inst.sprite->set_visible(false); } // no magic refill from fire
            } else if(invuln == 0 && logic::aabb_overlap(player.body, inst.e.body)){
                health.damage(20); invuln = 45;
            }
        }
        fire.despawn_on_solid(lvl.map);

        // ---- lava ----
        if(invuln == 0 && logic::lava_overlap(player.body, lvl.map)){ health.damage(20); invuln = 45; }

        // ---- pushable blocks: push detection, gravity, sprite ----
        if(push_cd > 0) --push_cd;
        for(BlockInst& bi : blocks){
            // push when grounded, holding a dir, and the tile in front of the player == this block
            if(push_cd == 0 && player.body.on_ground && (in.left || in.right)){
                int dir = in.right ? 1 : -1;
                int lead_px = in.right ? player.body.pos.x.to_int() + 16 : player.body.pos.x.to_int() - 1;
                int feet_ty = px2t(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1));
                if(px2t(fx(lead_px)) == bi.blk.tx && feet_ty == bi.blk.ty){
                    int oldx = bi.blk.tx;
                    if(bi.blk.push(dir, lvl.map)){
                        engine::set_collision_tile(oldx, bi.blk.ty, 0);
                        engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);
                        push_cd = 8;
                    }
                }
            }
            int oldy = bi.blk.ty;
            if(bi.blk.apply_gravity_step(lvl.map)){
                engine::set_collision_tile(bi.blk.tx, oldy, 0);
                engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);
            }
            int bx = bi.blk.tx * 8 + 4, by = bi.blk.ty * 8 + 4;
            bi.sprite->set_position(bx - hw, by - hh);
        }

        // ---- triggers: update inputs, open targets once ----
        for(TriggerInst& ti : triggers){
            switch(ti.trig.kind){
                case logic::TriggerKind::Plate: {
                    bool on = px2t(player.body.pos.x) <= ti.src_tx &&
                              px2t(player.body.pos.x + player.body.half_w + player.body.half_w - fx(1)) >= ti.src_tx &&
                              px2t(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1)) == ti.src_ty;
                    for(BlockInst& bi : blocks) if(bi.blk.tx == ti.src_tx && bi.blk.ty == ti.src_ty) on = true;
                    ti.trig.pressed = on;
                    break; }
                case logic::TriggerKind::Button: {
                    if(logic::aabb_overlap(player.body, tile_body(ti.src_tx, ti.src_ty, 4, 4))) ti.trig.pressed = true; // latch
                    break; }
                case logic::TriggerKind::Braziers: {
                    int n = 0; for(BrazierInst& bi : braziers) if(bi.group == ti.group && bi.lit) ++n;
                    ti.trig.lit = n;
                    break; }
            }
            if(!ti.applied && ti.trig.active()){
                ti.applied = true;
                open_region(lvl.view, ti.trig.target_tx, ti.trig.target_ty); // 2x4 passage
            }
        }

        // ---- i-frames / respawn ----
        if(invuln > 0) { --invuln; avatar.set_visible((invuln / 4) % 2 == 0); }
        else avatar.set_visible(true);
        if(health.is_empty()){
            player.body.pos = spawn_pos; player.body.vel = { fx(0), fx(0) };
            health.cur = health.max; invuln = 0;
        }

        // ---- ability shrines ----
        for(ShrineInst& si2 : shrines){
            if(!world.has(si2.pk.ability) && logic::aabb_overlap(player.body, si2.body)){
                world.grant(si2.pk.ability);
                spell.refresh(world);
                if(si2.sprite) si2.sprite->set_visible(false);
            }
        }
        spell_icon.set_visible(spell.selected == logic::SpellId::Fire);

        // ---- spronk rescue (marks cleared; Featherleap reward handled in M2/Task 4.3) ----
        if(level.has_cage){
            bool was = world.spronk_freed(d);
            logic::try_free_spronk(player.body, cage, world, d);
            if(world.spronk_freed(d) && !was){
                world.grant(logic::Ability::Featherleap);
                if(spronk) spronk->set_visible(false);
            }
        }

        bool spronk_ok = !level.has_cage || world.spronk_freed(d);
        if(level.has_exit && spronk_ok && logic::aabb_overlap(player.body, exit)){
            result = DungeonResult::Cleared; break;
        }

        hud.update(health, magic);
        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        cam.set_position(cx - hw, cy - hh);
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }

    engine::fade_out(16);
    return result;
}
}
