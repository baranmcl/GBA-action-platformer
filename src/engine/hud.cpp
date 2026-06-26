#include "engine/hud.h"
#include "logic/hud_math.h"
#include "bn_string.h"
#include "common_variable_8x8_sprite_font.h"   // compact font so the "xN" count fits one HUD row
#include "bn_sprite_items_hud_health.h"
#include "bn_sprite_items_hud_magic.h"
#include "bn_sprite_items_hud_life.h"
namespace engine {

Hud::Hud()
    // The lives icon + the count's text generator are not default-constructible, so build them here.
    : _life_icon(bn::sprite_items::hud_life.create_sprite(-116, -52)),
      _text_gen(common::variable_8x8_sprite_font)
{
    // Health row: pre-create the full variable-length bar (one slot per possible pip up to the
    // 150-HP cap). update() lights `fill` of them; the lit count is the bar's apparent length, so a
    // heart container (more max HP) lights MORE pips => a longer bar.
    for(int i = 0; i < HEALTH_PIPS; ++i)
        _health.push_back(bn::sprite_items::hud_health.create_sprite(-116 + i * 8, -72));
    for(int i = 0; i < PIPS; ++i)
        _magic.push_back(bn::sprite_items::hud_magic.create_sprite(-116 + i * 8, -62));
    // Lives row (y=-52): a single gold shield (created above) + an "xN" count just to its right
    // (left-aligned 8x8 text), so the readout is "shield x5" instead of five separate icons.
    _text_gen.set_left_alignment();
}

void Hud::update(const logic::Meter& health, const logic::Meter& magic, int lives){
    int hp  = logic::health_fill_pips(health.cur, health.max); // 0..HEALTH_PIPS lit, scales with max
    int mp  = logic::bar_pixels(magic.cur, magic.max, PIPS);
    for(int i = 0; i < HEALTH_PIPS; ++i)
        _health[i].set_visible(i < hp);
    for(int i = 0; i < PIPS; ++i)
        _magic[i].set_visible(i < mp);
    // Lives: regenerate the "xN" glyphs only when the count changes (text generation allocates).
    if(lives != _last_lives){
        int shown = lives < 0 ? 0 : lives;
        bn::string<8> s("x");
        s.append(bn::to_string<4>(shown));
        _life_text.clear();
        _text_gen.generate(-106, -55, s, _life_text);   // immediately right of the shield at x=-116
        _last_lives = lives;
    }
}
}
