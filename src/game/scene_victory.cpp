#include "game/scene_victory.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "common_variable_8x16_sprite_font.h"
#include "engine/fade.h"

namespace game
{
// The "THE END" finale screen. Modeled on run_title (text-gen + engine fade);
// returns when START is pressed so main.cpp can fall through to the title.
void run_victory(const logic::World& world)
{
    (void)world; // reserved for a flourish line; kept for signature parity / future use
    bn::bg_palettes::set_transparent_color(bn::color(2, 2, 10));

    bn::sprite_text_generator text(common::variable_8x16_sprite_font);
    text.set_center_alignment();

    bn::vector<bn::sprite_ptr, 24> the_end;
    text.generate(0, -32, "THE END", the_end);

    bn::vector<bn::sprite_ptr, 40> congrats;
    text.generate(0, 0, "CONGRATULATIONS", congrats);

    bn::vector<bn::sprite_ptr, 40> flourish;
    text.generate(0, 24, "ALL 8 SPRONKS FREED", flourish);

    bn::vector<bn::sprite_ptr, 40> prompt;
    text.generate(0, 56, "PRESS START", prompt);

    engine::fade_in(16); // fade in from the black handed over by run_boss

    while(true)
    {
        if(bn::keypad::start_pressed())
        {
            engine::fade_out(16); // hand the next scene a black screen
            return;
        }
        bn::core::update();
    }
}
}
