#include "engine/fade.h"
#include "bn_core.h"
#include "bn_fixed.h"
#include "bn_color.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_palettes.h"
namespace engine {

void set_fade(int level16){
    if(level16 < 0) level16 = 0;
    if(level16 > 16) level16 = 16;
    bn::fixed t = bn::fixed(level16) / 16;
    bn::bg_palettes::set_fade_color(bn::color(0, 0, 0));
    bn::sprite_palettes::set_fade_color(bn::color(0, 0, 0));
    bn::bg_palettes::set_fade_intensity(t);
    bn::sprite_palettes::set_fade_intensity(t);
}

namespace {
    void ramp(int from16, int to16, int frames){
        if(frames < 1) frames = 1;
        for(int i = 0; i <= frames; ++i){
            set_fade(from16 + (to16 - from16) * i / frames);
            bn::core::update();
        }
    }
}
void fade_out(int frames){ ramp(0, 16, frames); }
void fade_in(int frames){ ramp(16, 0, frames); }
}
