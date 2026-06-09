#pragma once
namespace logic {
// Blink (Dash) — a double-tap directional i-frame burst (M6). Pure logic, fed the held left/right
// flags once per frame; it tracks press edges itself (no new InputFrame fields). The Player applies
// the dash velocity while active(). One air-dash per airtime: the charge refreshes while grounded
// (so ground dashes are repeatable) and is spent when a dash starts.
struct DashState {
    static constexpr int WINDOW = 12;     // frames to complete a double-tap (feel-tunable)
    static constexpr int DASH_FRAMES = 7; // dash duration (~5 tiles at DASH_VX) (feel-tunable)
    int dash_frames = 0, dash_dir = 0;
    bool charged = false;             // one dash per airtime; refreshed while grounded
    int tap_dir = 0, tap_timer = 0;   // pending first-tap direction + window
    bool pl = false, pr = false;      // previous left/right held

    void tick(bool left, bool right, bool on_ground, bool has_dash){
        if(on_ground) charged = true;             // recharge while grounded (ground dashes repeatable; one air-dash/airtime)
        if(dash_frames > 0) --dash_frames;
        if(tap_timer > 0) --tap_timer;
        bool press_l = left && !pl, press_r = right && !pr;
        if(press_l) arm_or_dash(-1, has_dash);
        if(press_r) arm_or_dash(1, has_dash);
        pl = left; pr = right;
    }
    bool active() const { return dash_frames > 0; }
    bool invincible() const { return dash_frames > 0; }
    int dir() const { return dash_dir; }
private:
    void arm_or_dash(int d, bool has_dash){
        if(tap_dir == d && tap_timer > 0 && has_dash && charged && dash_frames == 0){
            dash_frames = DASH_FRAMES; dash_dir = d; charged = false; tap_timer = 0;
        } else { tap_dir = d; tap_timer = WINDOW; }
    }
};
}
