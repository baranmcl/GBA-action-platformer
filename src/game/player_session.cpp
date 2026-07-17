#include "game/player_session.h"

#include "bn_sprite_items_fire_proj.h"
#include "bn_sprite_items_ice_proj.h"
#include "bn_sprite_items_grapple_icon.h"
#include "bn_sprite_items_light_proj.h"
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_magic_crystal.h"

#include "logic/collision.h"   // aabb_overlap
#include "logic/tilemap.h"     // Tilemap (CrystalStation::spawn's FloorBelow grounding scan)
#include "logic/meters.h"      // Meter
#include "engine/input.h"        // read_input, read_aim_dy
#include "engine/spell_input.h"  // read_spell_intent

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }

    // Verbatim copy of scene_dungeon.cpp's tile_body helper (pure logic — no bn:: types — so
    // duplicating it here rather than sharing across TUs is semantics-preserving and harmless).
    logic::Body tile_body(int tx, int ty, int hw, int hh){
        logic::Body b{}; b.half_w = fx(hw); b.half_h = fx(hh);
        b.pos = { fx(tx * 8), fx(ty * 8) }; return b;
    }

    // Verbatim copy of scene_dungeon.cpp's floor_row_below helper (see tile_body's note above).
    int floor_row_below(const logic::Tilemap& map, int tx, int start_ty){
        for(int y = start_ty + 1; y < map.h; ++y)
            if(map.is_solid(tx, y) || map.is_oneway(tx, y)) return y;   // standable: solid or one-way platform
        return start_ty + 1;
    }
}

// ---------------------------------------------------------------------------
// set_clamped_cam — moved verbatim from play_room's lambda (scene_dungeon.cpp); hw/hh are now
// derived from the map_px_w/map_px_h params instead of being closed over.
void set_clamped_cam(bn::camera_ptr& cam, int map_px_w, int map_px_h,
                     int level_w, int level_h, int cx, int cy)
{
    const int hw = map_px_w / 2, hh = map_px_h / 2;
    const int ll = -hw, lt = -hh, lr = ll + level_w * 8, lb = lt + level_h * 8;
    const int minx = ll + 120, maxx = lr - 120, miny = lt + 80, maxy = lb - 80;
    int camx = cx - hw, camy = cy - hh;
    camx = (minx <= maxx) ? (camx < minx ? minx : camx > maxx ? maxx : camx) : (ll + lr) / 2;
    camy = (miny <= maxy) ? (camy < miny ? miny : camy > maxy ? maxy : camy) : (lt + lb) / 2;
    cam.set_position(camx, camy);
}

// ---------------------------------------------------------------------------
PlayerSession::PlayerSession(bn::camera_ptr& cam, logic::World& world, logic::PlayerState& ps, logic::Player& player)
    : _cam(cam), _world(world), _ps(ps), _player(player),
      _spell_icon(bn::sprite_items::fire_proj.create_sprite(104, -68))
{
    constexpr int VINE_SEGS = 4;
    for(int i = 0; i < VINE_SEGS; ++i){
        _vine_segs.push_back(bn::sprite_items::bolt.create_sprite(0, 0));
        _vine_segs.back().set_camera(_cam);
        _vine_segs.back().set_visible(false);
        _vine_segs.back().set_scale(0.5);
    }
}

// Moved verbatim from play_room/run_room_boss's 5-line ability-sync block.
void PlayerSession::sync_abilities()
{
    _player.abilities.featherleap = _world.has(logic::Ability::Featherleap);
    _player.abilities.glide       = _world.has(logic::Ability::Glide);
    _player.abilities.dash        = _world.has(logic::Ability::Dash);
    _player.abilities.grapple     = _world.has(logic::Ability::Grapple);
    _player.abilities.stone       = _world.has(logic::Ability::Stone);
}

// Moved verbatim from play_room's intent-decode (886-890, 949-951); run_room_boss's variant sets
// in.grapple_fire = want_grapple directly since the arena has no blocks/enemies to target — that
// difference is normalized to the dungeon form here (in.grapple_fire=false), the caller (a boss
// loop) is responsible for setting it to want_grapple since it has no pull targets of its own.
SessionIntent PlayerSession::read_intent()
{
    SessionIntent si;
    si.in = engine::read_input();
    engine::SpellIntent spell_in = engine::read_spell_intent();
    if(spell_in.cycle) _ps.spell.cycle(_world);
    si.want_grapple = spell_in.cast && _ps.spell.selected == logic::SpellId::Grapple;
    si.in.grapple_fire = false;
    si.cast_spell = spell_in.cast && (_ps.spell.selected == logic::SpellId::Fire ||
                                      _ps.spell.selected == logic::SpellId::Ice  ||
                                      _ps.spell.selected == logic::SpellId::Light);
    return si;
}

// Moved verbatim from play_room's refresh_spell_icon lambda.
void PlayerSession::refresh_spell_icon()
{
    if(_ps.spell.selected != _last_icon){
        if(_ps.spell.selected == logic::SpellId::Ice)          _spell_icon.set_item(bn::sprite_items::ice_proj);
        else if(_ps.spell.selected == logic::SpellId::Fire)    _spell_icon.set_item(bn::sprite_items::fire_proj);
        else if(_ps.spell.selected == logic::SpellId::Grapple) _spell_icon.set_item(bn::sprite_items::grapple_icon);
        else if(_ps.spell.selected == logic::SpellId::Light)   _spell_icon.set_item(bn::sprite_items::light_proj);
        _last_icon = _ps.spell.selected;
    }
    _spell_icon.set_visible(_ps.spell.selected != logic::SpellId::None);
}

// Moved verbatim from play_room's shot-aim muzzle calc (:1068-1069/1070-1071).
logic::Vec2 PlayerSession::muzzle() const
{
    return { _player.body.pos.x + _player.body.half_w,
             _player.body.pos.y + _player.body.half_h + fx(engine::read_aim_dy()) };
}

void PlayerSession::note_anchor_miss(int facing)
{
    _miss_vine_t = 10;
    _miss_vine_dir = facing;
}

// Moved verbatim from play_room's vine VFX block (:1322-1364); hw/hh are now params instead of
// closed-over locals, per the brief's update_vine_vfx(int,int) signature.
void PlayerSession::update_vine_vfx(int hw, int hh)
{
    constexpr int VINE_SEGS = 4;
    if(_miss_vine_t > 0) --_miss_vine_t;
    if(_player.grapple.active()){
        // Latched: draw dots from player to anchor (unchanged).
        int px_ = _player.body.pos.x.to_int() + _player.body.half_w.to_int();
        int py_ = _player.body.pos.y.to_int() + _player.body.half_h.to_int();
        int ax_ = _player.grapple.anchor_tx * 8 + 4;
        int ay_ = _player.grapple.anchor_ty * 8 + 4;
        for(int i = 0; i < VINE_SEGS; ++i){
            int t = i + 1;  // t in 1..VINE_SEGS (skip the player pos itself)
            int sx_ = px_ + (ax_ - px_) * t / (VINE_SEGS + 1);
            int sy_ = py_ + (ay_ - py_) * t / (VINE_SEGS + 1);
            _vine_segs[i].set_position(sx_ - hw, sy_ - hh);
            _vine_segs[i].set_visible(true);
        }
    } else if(_miss_vine_t > 0){
        // Miss: shoot vine out and retract. Duration 10 frames; peaks at frame 5.
        int elapsed = 10 - _miss_vine_t;           // 0..9 as the timer runs from 10 down to 1
        int half = 5;
        int reach_tiles;
        if(elapsed < half){
            reach_tiles = (logic::GrappleState::RANGE * (elapsed + 1)) / half; // 0->RANGE
        } else {
            reach_tiles = (logic::GrappleState::RANGE * (10 - elapsed)) / half; // RANGE->0
        }
        if(reach_tiles < 1) reach_tiles = 1; // always at least one segment visible while active
        int px_ = _player.body.pos.x.to_int() + _player.body.half_w.to_int();
        int py_ = _player.body.pos.y.to_int() + _player.body.half_h.to_int();
        int reach_px = reach_tiles * 8;
        for(int i = 0; i < VINE_SEGS; ++i){
            int t = i + 1;
            int sx_ = px_ + (_miss_vine_dir * reach_px * t) / (VINE_SEGS + 1);
            int sy_ = py_;  // horizontal shot (stays at player centre height)
            _vine_segs[i].set_position(sx_ - hw, sy_ - hh);
            _vine_segs[i].set_visible(true);
        }
    } else {
        for(int i = 0; i < VINE_SEGS; ++i) _vine_segs[i].set_visible(false);
    }
}

// ---------------------------------------------------------------------------
// CrystalStation — merges play_room's magic_crystals loop (TileCentre grounding,
// respawn_when_depleted=false) and run_room_boss's single-crystal loop (FloorBelow grounding,
// respawn_when_depleted=true), per the brief's Grounding enum + flag.
void CrystalStation::spawn(const logic::LevelData& level, bn::camera_ptr& cam, const logic::Tilemap& map,
                           int hw, int hh, Grounding g, bool respawn_when_depleted)
{
    _crystals.clear();
    _respawn_when_depleted = respawn_when_depleted;
    for(int i = 0; i < level.magic_crystal_count && i < 8; ++i){
        const logic::MagicCrystalSpawn& mc = level.magic_crystals[i];
        _crystals.push_back(Crystal{ tile_body(mc.tx, mc.ty, 6, 8), {}, false });
        Crystal& c = _crystals.back();
        c.sprite = bn::sprite_items::magic_crystal.create_sprite(0, 0);
        c.sprite->set_camera(cam);
        // TileCentre (play_room): mc.ty*8+8 — the authored tile's own centre row.
        // FloorBelow (boss loops): the first standable row below, minus half the sprite height —
        // grounds the sprite on the floor surface instead of sinking it in (the M13 fix).
        int y_px = (g == Grounding::TileCentre) ? (mc.ty * 8 + 8)
                                                 : (floor_row_below(map, mc.tx, mc.ty) * 8 - 8);
        c.sprite->set_position(mc.tx * 8 + 8 - hw, y_px - hh);
    }
}

void CrystalStation::update(const logic::Body& player_body, logic::Meter& magic)
{
    for(Crystal& c : _crystals){
        if(!c.collected && logic::aabb_overlap(player_body, c.body)){
            magic.cur = magic.max;   // full refill — the guaranteed combat-free magic source
            c.collected = true;
            if(c.sprite) c.sprite->set_visible(false);
        }
        if(_respawn_when_depleted && c.collected && magic.cur < logic::SPELL_COST){
            c.collected = false;
            if(c.sprite) c.sprite->set_visible(true);
        }
    }
}

void CrystalStation::reset()
{
    for(Crystal& c : _crystals){
        c.collected = false;
        if(c.sprite) c.sprite->set_visible(true);
    }
}
}
