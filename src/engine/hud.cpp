#include "engine/hud.h"
#include "logic/hud_math.h"
#include "bn_sprite_items_hud_health.h"
#include "bn_sprite_items_hud_magic.h"
namespace engine {

Hud::Hud(){
    // Health row: pre-create the full variable-length bar (one slot per possible pip up to
    // the 150-HP cap). update() lights `fill` of them; the lit count is the bar's apparent
    // length, so grabbing a heart container (more max HP) lights MORE pips => a longer bar.
    for(int i = 0; i < HEALTH_PIPS; ++i)
        _health.push_back(bn::sprite_items::hud_health.create_sprite(-116 + i * 8, -72));
    for(int i = 0; i < PIPS; ++i)
        _magic.push_back(bn::sprite_items::hud_magic.create_sprite(-116 + i * 8, -62));
}

void Hud::update(const logic::Meter& health, const logic::Meter& magic){
    int hp = logic::health_fill_pips(health.cur, health.max); // 0..HEALTH_PIPS lit, scales with max
    int mp = logic::bar_pixels(magic.cur, magic.max, PIPS);
    for(int i = 0; i < HEALTH_PIPS; ++i)
        _health[i].set_visible(i < hp);
    for(int i = 0; i < PIPS; ++i)
        _magic[i].set_visible(i < mp);
}
}
