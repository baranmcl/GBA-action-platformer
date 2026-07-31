#include "game/scene_title.h"

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
TitleResult run_title(const logic::World& world)
{
    bn::bg_palettes::set_transparent_color(bn::color(4, 6, 22));

    bn::sprite_text_generator text(common::variable_8x16_sprite_font);
    text.set_center_alignment();

    bn::vector<bn::sprite_ptr, 24> title;
    text.generate(0, -40, "SPRONK QUEST", title);

    bn::vector<bn::sprite_ptr, 40> prompt;
    text.generate(0, 16, world.spronks_freed != 0 ? "CONTINUE - PRESS START" : "PRESS START", prompt);

    // M11: reflect a completed game with a single additive line (does not alter title/prompt/START).
    bn::vector<bn::sprite_ptr, 24> complete;
    if(world.beaten) text.generate(0, 40, "GAME COMPLETE", complete);

    engine::fade_in(16); // fade in from the black handed over by the previous scene

    while(true)
    {
        if(bn::keypad::start_pressed())
        {
            // I35 dev-tooling combo: holding SELECT while pressing START enters the debug
            // dungeon/ability selector instead of the hub. Checked only on the START edge (not
            // select_pressed) so SELECT can be picked up first or simultaneously with START.
            bool debug = bn::keypad::select_held();
            engine::fade_out(16); // hand the next scene a black screen
            return TitleResult{ debug };
        }
        bn::core::update();
    }
}
}
