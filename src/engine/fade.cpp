#include "engine/fade.h"
#include "bn_core.h"
#include "bn_fixed.h"
#include "bn_color.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_palettes.h"
namespace engine {
namespace {
    void ramp(bn::fixed from, bn::fixed to, int frames){
        if(frames < 1) frames = 1;
        bn::bg_palettes::set_fade_color(bn::color(0, 0, 0));
        bn::sprite_palettes::set_fade_color(bn::color(0, 0, 0));
        for(int i = 0; i <= frames; ++i){
            bn::fixed t = from + (to - from) * i / frames;
            bn::bg_palettes::set_fade_intensity(t);
            bn::sprite_palettes::set_fade_intensity(t);
            bn::core::update();
        }
    }
}
void fade_out(int frames){ ramp(0, 1, frames); }
void fade_in(int frames){ ramp(1, 0, frames); }
}
