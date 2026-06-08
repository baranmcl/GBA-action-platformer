#include "engine/spell_pool.h"
#include "logic/collision.h"
#include "bn_sprite_items_fire_proj.h"
#include "bn_sprite_items_ice_proj.h"
namespace engine {

SpellPool::SpellPool(int map_px_w, int map_px_h, const bn::camera_ptr& cam)
    : _half_w_px(map_px_w / 2), _half_h_px(map_px_h / 2), _camera(cam) {}

void SpellPool::update_and_cast(bool cast_pressed, logic::SpellState& spell, logic::Meter& magic,
                                logic::Vec2 muzzle, int facing, const logic::Tilemap& map){
    _caster.tick();
    if(spell.selected != logic::SpellId::None){
        logic::BoltSpawn spawn;
        if(_caster.try_cast(cast_pressed, magic, muzzle, facing, spawn)){
            for(Shot& s : _shots){
                if(!s.active){
                    s.active = true;
                    s.kind = spell.selected;            // tag the shot with the spell it was cast as
                    s.body.pos = spawn.pos;
                    s.body.half_w = logic::Fixed::from_int(3);
                    s.body.half_h = logic::Fixed::from_int(3);
                    s.vel = spawn.vel;
                    s.sprite = (spell.selected == logic::SpellId::Ice)
                             ? bn::sprite_items::ice_proj.create_sprite(0, 0)
                             : bn::sprite_items::fire_proj.create_sprite(0, 0);
                    s.sprite->set_camera(_camera);
                    break;
                }
            }
        }
    }

    const int level_w = map.w * logic::Tilemap::TILE;
    const int level_h = map.h * logic::Tilemap::TILE;
    for(Shot& s : _shots){
        if(!s.active) continue;
        s.body.pos = s.body.pos + s.vel;
        int x = s.body.pos.x.to_int();
        int y = s.body.pos.y.to_int();
        if(x < 0 || x >= level_w || y < 0 || y >= level_h){ // OFF-LEVEL only (walls handled by despawn_on_solid)
            s.active = false; s.sprite.reset(); continue;
        }
        int cx = x + s.body.half_w.to_int();
        int cy = y + s.body.half_h.to_int();
        s.sprite->set_position(cx - _half_w_px, cy - _half_h_px);
    }
}

bool SpellPool::consume_hit(const logic::Body& target, logic::SpellId kind){
    for(Shot& s : _shots){
        if(s.active && s.kind == kind && logic::aabb_overlap(s.body, target)){
            s.active = false; s.sprite.reset(); return true;
        }
    }
    return false;
}

bool SpellPool::consume_tile_hit(const logic::Tilemap& map, logic::TileKind target_kind,
                                 logic::SpellId spell_kind, int& out_tx, int& out_ty){
    for(Shot& s : _shots){
        if(!s.active || s.kind != spell_kind) continue;
        int tx = logic::Tilemap::px_to_tile(s.body.pos.x + s.body.half_w);
        int ty = logic::Tilemap::px_to_tile(s.body.pos.y + s.body.half_h);
        for(int y = ty; y < map.h; ++y){
            if(map.at(tx, y) == target_kind){       // matched (melt tests IcePlatform before the solid break)
                s.active = false; s.sprite.reset(); out_tx = tx; out_ty = y; return true;
            }
            if(map.is_solid(tx, y)) break;           // a wall (or ice when freezing) stops the column scan
        }
    }
    return false;
}

void SpellPool::despawn_on_solid(const logic::Tilemap& map){
    for(Shot& s : _shots){
        if(!s.active) continue;
        if(map.is_solid(logic::Tilemap::px_to_tile(s.body.pos.x),
                        logic::Tilemap::px_to_tile(s.body.pos.y))){
            s.active = false; s.sprite.reset();
        }
    }
}
}
