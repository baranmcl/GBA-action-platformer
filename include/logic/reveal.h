#pragma once
namespace logic {
// M10 Light reveal: a room-wide timer. A Light cast (re)starts it; while >0 the room's hidden
// platforms are solid+visible (the scene toggles collision/sprites). Pure logic, integer-only.
struct RevealState {
    static constexpr int REVEAL_FRAMES = 200;  // ~3.3s at 60fps (feel-tunable in QA)
    int timer = 0;
    void on_cast(){ timer = REVEAL_FRAMES; }
    void tick(){ if(timer > 0) --timer; }
    bool revealed() const { return timer > 0; }
};
}
