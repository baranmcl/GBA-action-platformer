#include "engine/avatar.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_sprite_tiles_item.h"
#include "bn_sprite_items_laurel.h"
namespace engine {

Avatar::Avatar(const logic::Player& p, int map_px_w, int map_px_h, const bn::camera_ptr& cam)
    : _sprite(bn::sprite_items::laurel.create_sprite(0, 0)),
      _half_w_px(map_px_w / 2),
      _half_h_px(map_px_h / 2)
{
    _sprite.set_camera(cam);
    sync(p);
}

void Avatar::sync(const logic::Player& p){
    // sprite centre (world) = body centre (level) - half map
    int cx = p.body.pos.x.to_int() + p.body.half_w.to_int();
    int cy = p.body.pos.y.to_int() + p.body.half_h.to_int();
    _sprite.set_position(cx - _half_w_px, cy - _half_h_px);
    _sprite.set_horizontal_flip(p.facing < 0);

    // frame: 2 jump (airborne), 1 run (moving on ground), else 0 idle
    int frame = 0;
    if(!p.body.on_ground){
        frame = 2;
    } else if(p.body.vel.x.raw != 0){
        frame = 1;
    }
    if(frame != _frame){
        _frame = frame;
        _sprite.set_tiles(bn::sprite_items::laurel.tiles_item().create_tiles(frame));
    }
}
}
