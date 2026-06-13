#pragma once
namespace logic {
// Stone ground-pound (M8): a locked, fast downward plunge with i-frames, started while airborne.
// Pure logic; the Player decides WHEN to start it (Down+A), the scene resolves WHAT it hits on landing.
struct StoneState {
    static constexpr int POUND_VY_RAW = 2048;  // = Fixed::from_int(8) = 8 px/frame; > the normal fall
        // terminal (PH terminal from_int(6)=1536). move_and_collide sub-steps at <=1 tile
        // (collision.cpp "no tunneling"), so this speed never tunnels through a thin floor (TEST-3).
    bool pounding = false;
    bool landed   = false;   // one-frame signal: became grounded this tick while pounding
    // Begin (or RE-ARM) a pound. The scene re-arms after smashing a cracked floor so one pound
    // plunges through STACKED cracked floors (see Phase 3). Idempotent if already pounding.
    void start(){ if(!pounding){ pounding = true; landed = false; } }
    bool active() const { return pounding; }
    bool invincible() const { return pounding; }   // i-frames like Dash
    bool just_landed() const { return landed; }
    // Called by Player::update with the post-move grounded state. Raises `landed` the frame the
    // pound hits ground; ends the pound. (The scene reads just_landed() that same frame.)
    void post(bool on_ground_after_move){
        landed = false;
        if(pounding && on_ground_after_move){ landed = true; pounding = false; }
    }
};
}
