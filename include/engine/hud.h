#pragma once
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "logic/meters.h"
#include "logic/hud_math.h"
namespace engine {
// Top-left HUD: two variable-length pip bars (health = red, magic = cyan) plus a LIVES readout shown
// as ONE gold-shield icon followed by an "xN" count (e.g. shield + "x5"), rather than one icon per
// life. Screen-fixed (no camera).
class Hud {
public:
    Hud();
    void update(const logic::Meter& health, const logic::Meter& magic, int lives);
private:
    static constexpr int PIPS = 10;                               // magic row: fixed ratio bar
    static constexpr int HEALTH_PIPS = logic::MAX_HEALTH_PIPS;    // health row: variable-length bar
    bn::vector<bn::sprite_ptr, HEALTH_PIPS> _health;
    bn::vector<bn::sprite_ptr, PIPS>        _magic;
    bn::sprite_ptr            _life_icon;   // single gold-shield icon (always shown)
    bn::sprite_text_generator _text_gen;    // renders the "xN" life count next to the icon
    bn::vector<bn::sprite_ptr, 4> _life_text;   // up to "x12" (3 glyphs) + margin
    int _last_lives = -1;                   // regenerate the count text only when it changes
};
}
