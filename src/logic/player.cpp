#include "logic/player.h"
#include "logic/wind.h"
namespace logic {
static const Fixed RUN_ACCEL = Fixed::from_raw(24);
static const Fixed RUN_MAX   = Fixed::from_int(2);
static const Fixed FRICTION  = Fixed::from_raw(24);
static const Fixed JUMP_VY   = Fixed::from_int(-4);
// M5 wind kit (starting values; mGBA tunes feel).
static const Fixed GLIDE_VY   = Fixed::from_int(1);   // gentle fall terminal while gliding (vs PH terminal 6)
static const Fixed UPDRAFT_VY = Fixed::from_int(-3);  // rise speed in an updraft while gliding (tuned up for lift margin)
static const Fixed WIND_ACCEL = Fixed::from_raw(24);  // per-frame sideways gust push (bounded by RUN_MAX next frame)
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
    // --- M5 wind kit (ORDER: after gravity sets the fall speed, before collision resolves it) ---
    if(abilities.glide && in.glide_held && !body.on_ground){
        if(updraft_overlap(body, map)) body.vel.y = UPDRAFT_VY;        // updraft OVERRIDES glide OVERRIDES gravity
        else if(body.vel.y > GLIDE_VY) body.vel.y = GLIDE_VY;          // otherwise glide (cap the fall)
    }
    int wd = wind_dir(body, map);                                     // gusts push regardless of ability
    if(wd > 0)      body.vel.x = body.vel.x + WIND_ACCEL;
    else if(wd < 0) body.vel.x = body.vel.x - WIND_ACCEL;
    move_and_collide(body, map);

    // refresh the air-jump charge whenever grounded (only if Featherleap is owned)
    if(body.on_ground) air_jumps_left = abilities.featherleap ? 1 : 0;
}
}
