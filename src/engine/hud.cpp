#include "engine/hud.h"
#include "logic/hud_math.h"
#include "bn_sprite_items_hud_health.h"
#include "bn_sprite_items_hud_magic.h"
#include "bn_sprite_items_hud_life.h"
namespace engine {

Hud::Hud(){
    // Health row: pre-create the full variable-length bar (one slot per possible pip up to
    // the 150-HP cap). update() lights `fill` of them; the lit count is the bar's apparent
    // length, so grabbing a heart container (more max HP) lights MORE pips => a longer bar.
    for(int i = 0; i < HEALTH_PIPS; ++i)
        _health.push_back(bn::sprite_items::hud_health.create_sprite(-116 + i * 8, -72));
    for(int i = 0; i < PIPS; ++i)
        _magic.push_back(bn::sprite_items::hud_magic.create_sprite(-116 + i * 8, -62));
    // Lives row: one gold-shield pip per current life; up to LIVES_PIPS slots pre-created.
    // Positioned one row below magic (-52) so all three rows are on-screen and non-overlapping.
    // At max lives (3 base + 8 spronks = 11) all 11 pips from x=-116 to x=-36 remain on-screen.
    for(int i = 0; i < LIVES_PIPS; ++i)
        _lives.push_back(bn::sprite_items::hud_life.create_sprite(-116 + i * 8, -52));
}

void Hud::update(const logic::Meter& health, const logic::Meter& magic, int lives){
    int hp  = logic::health_fill_pips(health.cur, health.max); // 0..HEALTH_PIPS lit, scales with max
    int mp  = logic::bar_pixels(magic.cur, magic.max, PIPS);
    int lp  = logic::lives_display_count(lives);               // 0..LIVES_HUD_CAP lit
    for(int i = 0; i < HEALTH_PIPS; ++i)
        _health[i].set_visible(i < hp);
    for(int i = 0; i < PIPS; ++i)
        _magic[i].set_visible(i < mp);
    for(int i = 0; i < LIVES_PIPS; ++i)
        _lives[i].set_visible(i < lp);
}
}
