#include "logic/player.h"
#include "logic/wind.h"
namespace logic {
static const Fixed RUN_ACCEL = Fixed::from_raw(24);
static const Fixed RUN_MAX   = Fixed::from_int(2);
static const Fixed FRICTION  = Fixed::from_raw(24);
static const Fixed ICE_FRICTION = Fixed::from_raw(4);  // frozen-water ice is slippery: you slide, don't stop dead
static const Fixed JUMP_VY   = Fixed::from_raw(-812);  // ~3.2 px -> single jump ~3.5 tiles (was -4 = 5.5; tighter platforming so Featherleap/Glide matter)
// M5 wind kit (starting values; mGBA tunes feel).
static const Fixed GLIDE_VY   = Fixed::from_int(1);   // gentle fall terminal while gliding (vs PH terminal 6)
static const Fixed UPDRAFT_VY = Fixed::from_int(-3);  // rise speed in an updraft while gliding (tuned up for lift margin)
static const Fixed WIND_ACCEL = Fixed::from_raw(24);  // per-frame sideways gust push (bounded by RUN_MAX next frame)
static const Fixed DASH_VX    = Fixed::from_int(6);   // M6 Blink burst speed (~6 px/frame; DASH_FRAMES -> ~5 tiles)
// Midpoint feel (playtested): single jump ~5.5 tiles. Gravity is halfway between the
// floaty (28) and heavy (64) trials; terminal kept gentle at 6.
static const PhysicsParams PH { Fixed::from_raw(46), Fixed::from_int(6) };

void Player::update(const InputFrame& in, const Tilemap& map){
    // Slippery ice: if grounded on a frozen-water IcePlatform tile, friction is much weaker, so
    // releasing the d-pad lets the player keep sliding instead of stopping dead. (Tile-under-feet
    // from last frame's resolved position.)
    bool on_ice = false;
    if(body.on_ground){
        int feet = Tilemap::px_to_tile(body.pos.y + body.half_h + body.half_h);
        int l = Tilemap::px_to_tile(body.pos.x);
        int r = Tilemap::px_to_tile(body.pos.x + body.half_w + body.half_w - Fixed::from_int(1));
        for(int tx=l; tx<=r; ++tx) if(map.at(tx,feet)==TileKind::IcePlatform){ on_ice = true; break; }
    }
    const Fixed fric = on_ice ? ICE_FRICTION : FRICTION;
    // horizontal acceleration / friction
    if(in.right){ body.vel.x = body.vel.x + RUN_ACCEL; facing = 1; }
    else if(in.left){ body.vel.x = body.vel.x - RUN_ACCEL; facing = -1; }
    else {
        if(body.vel.x.raw > 0){ body.vel.x = body.vel.x - fric; if(body.vel.x.raw < 0) body.vel.x = Fixed::from_int(0); }
        else if(body.vel.x.raw < 0){ body.vel.x = body.vel.x + fric; if(body.vel.x.raw > 0) body.vel.x = Fixed::from_int(0); }
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
    // --- M6 dash: a horizontal i-frame blink; overrides accel/friction/gravity/wind while active ---
    // (applied AFTER the wind kit so a gust can't add to the dash velocity)
    dash.tick(in.left, in.right, body.on_ground, abilities.dash);
    if(dash.active()){
        body.vel.x = (dash.dir() > 0) ? DASH_VX : Fixed::from_raw(-DASH_VX.raw); // no Fixed::operator*(int): sign-branch
        body.vel.y = Fixed::from_int(0);
    }
    // --- M7 vine grapple: latch onto an anchor and pull the body toward it; overrides
    // accel/friction/gravity/dash while active (applied last so it wins). ---
    grapple.latch(in.grapple_fire, body, facing, map, abilities.grapple);
    if(grapple.active()){
        body.vel = grapple.pull_velocity(body);
    }
    Vec2 grapple_prev = body.pos;  // snapshot before move_and_collide so post() can detect a wall-stop
    move_and_collide(body, map);
    if(grapple.active()){
        bool moved = body.pos.x.raw != grapple_prev.x.raw || body.pos.y.raw != grapple_prev.y.raw;
        grapple.post(body, moved);
    }

    // refresh the air-jump charge whenever grounded (only if Featherleap is owned)
    if(body.on_ground) air_jumps_left = abilities.featherleap ? 1 : 0;
}
}
