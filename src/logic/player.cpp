#include "logic/player.h"
namespace logic {
static const Fixed RUN_ACCEL = Fixed::from_raw(24);
static const Fixed RUN_MAX   = Fixed::from_int(2);
static const Fixed FRICTION  = Fixed::from_raw(24);
static const Fixed JUMP_VY   = Fixed::from_int(-4);
// Midpoint feel (playtested): single jump ~5.5 tiles. Gravity is halfway between the
// floaty (28) and heavy (64) trials; terminal kept gentle at 6.
static const PhysicsParams PH { Fixed::from_raw(46), Fixed::from_int(6) };

void Player::update(const InputFrame& in, const Tilemap& map){
    // horizontal acceleration / friction
    if(in.right){ body.vel.x = body.vel.x + RUN_ACCEL; facing = 1; }
    else if(in.left){ body.vel.x = body.vel.x - RUN_ACCEL; facing = -1; }
    else {
        if(body.vel.x.raw > 0){ body.vel.x = body.vel.x - FRICTION; if(body.vel.x.raw < 0) body.vel.x = Fixed::from_int(0); }
        else if(body.vel.x.raw < 0){ body.vel.x = body.vel.x + FRICTION; if(body.vel.x.raw > 0) body.vel.x = Fixed::from_int(0); }
    }
    if(body.vel.x > RUN_MAX) body.vel.x = RUN_MAX;
    else if(body.vel.x < -RUN_MAX) body.vel.x = -RUN_MAX;

    // jump: ground jump, or ability-gated air (double) jump
    if(in.jump_pressed){
        if(body.on_ground){ body.vel.y = JUMP_VY; }
        else if(abilities.featherleap && air_jumps_left > 0){ body.vel.y = JUMP_VY; --air_jumps_left; }
    }

    apply_gravity(body, PH);
    move_and_collide(body, map);

    // refresh the air-jump charge whenever grounded (only if Featherleap is owned)
    if(body.on_ground) air_jumps_left = abilities.featherleap ? 1 : 0;
}
}
