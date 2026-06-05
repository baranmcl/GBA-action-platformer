#include "game/scene_title.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "common_variable_8x16_sprite_font.h"

namespace game
{
void run_title(const logic::World& world)
{
    bn::bg_palettes::set_transparent_color(bn::color(4, 6, 22));

    bn::sprite_text_generator text(common::variable_8x16_sprite_font);
    text.set_center_alignment();

    bn::vector<bn::sprite_ptr, 24> title;
    text.generate(0, -40, "SPRONK QUEST", title);

    bn::vector<bn::sprite_ptr, 40> prompt;
    text.generate(0, 16, world.spronks_freed != 0 ? "CONTINUE - PRESS START" : "PRESS START", prompt);

    while(true)
    {
        if(bn::keypad::start_pressed()) return;
        bn::core::update();
    }
}
}
