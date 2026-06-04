#include "logic/enemy.h"
namespace logic {
void Enemy::update(const Tilemap& map){
    if(!alive) return;
    static const Fixed SPEED { Fixed::from_raw(64) };               // 0.25 px/tick
    static const PhysicsParams PH { Fixed::from_raw(46), Fixed::from_int(6) };

    if(body.pos.x <= left_bound) dir = 1;
    else if(body.pos.x >= right_bound) dir = -1;
    body.vel.x = (dir > 0) ? SPEED : -SPEED;

    apply_gravity(body, PH);
    Fixed before_x = body.pos.x;
    move_and_collide(body, map);
    if(body.pos.x == before_x) dir = -dir; // blocked by a wall -> turn around
}
}
