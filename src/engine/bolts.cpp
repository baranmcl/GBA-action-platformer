#include "engine/bolts.h"
#include "bn_sprite_items_bolt.h"
namespace engine {

BoltPool::BoltPool(int map_px_w, int map_px_h, const bn::camera_ptr& cam)
    : _half_w_px(map_px_w / 2), _half_h_px(map_px_h / 2), _camera(cam) {}

void BoltPool::update(bool fire_pressed, logic::Vec2 muzzle, int facing, const logic::Tilemap& map){
    _caster.tick();

    logic::BoltSpawn spawn;
    if(_caster.try_fire(fire_pressed, muzzle, facing, spawn)){
        for(Bolt& b : _bolts){
            if(!b.active){
                b.active = true;
                b.body.pos = spawn.pos;
                b.body.half_w = logic::Fixed::from_int(2);
                b.body.half_h = logic::Fixed::from_int(2);
                b.vel = spawn.vel;
                b.sprite = bn::sprite_items::bolt.create_sprite(0, 0);
                b.sprite->set_camera(_camera);
                break;
            }
        }
    }

    const int level_w = map.w * logic::Tilemap::TILE;
    const int level_h = map.h * logic::Tilemap::TILE;
    for(Bolt& b : _bolts){
        if(!b.active) continue;
        b.body.pos = b.body.pos + b.vel;

        int x = b.body.pos.x.to_int();
        int y = b.body.pos.y.to_int();
        bool off = x < 0 || x >= level_w || y < 0 || y >= level_h;
        bool wall = !off && map.is_solid(logic::Tilemap::px_to_tile(b.body.pos.x),
                                         logic::Tilemap::px_to_tile(b.body.pos.y));
        if(off || wall){
            b.active = false;
            b.sprite.reset();
            continue;
        }
        int cx = x + b.body.half_w.to_int();
        int cy = y + b.body.half_h.to_int();
        b.sprite->set_position(cx - _half_w_px, cy - _half_h_px);
    }
}
}
