#pragma once
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "logic/meters.h"
namespace engine {
// Top-left HUD: two rows of pips (health = red, magic = cyan). Screen-fixed (no camera).
class Hud {
public:
    Hud();
    void update(const logic::Meter& health, const logic::Meter& magic);
private:
    static constexpr int PIPS = 10;
    bn::vector<bn::sprite_ptr, PIPS> _health;
    bn::vector<bn::sprite_ptr, PIPS> _magic;
};
}
