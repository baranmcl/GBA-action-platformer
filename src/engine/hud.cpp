#include "engine/hud.h"
#include "logic/hud_math.h"
#include "bn_sprite_items_hud_health.h"
#include "bn_sprite_items_hud_magic.h"
namespace engine {

Hud::Hud(){
    for(int i = 0; i < PIPS; ++i){
        _health.push_back(bn::sprite_items::hud_health.create_sprite(-116 + i * 8, -72));
        _magic.push_back(bn::sprite_items::hud_magic.create_sprite(-116 + i * 8, -62));
    }
}

void Hud::update(const logic::Meter& health, const logic::Meter& magic){
    int hp = logic::bar_pixels(health.cur, health.max, PIPS); // 0..PIPS filled
    int mp = logic::bar_pixels(magic.cur, magic.max, PIPS);
    for(int i = 0; i < PIPS; ++i){
        _health[i].set_visible(i < hp);
        _magic[i].set_visible(i < mp);
    }
}
}
