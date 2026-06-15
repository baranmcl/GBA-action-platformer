#include "game/scene_game_over.h"

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
GameOverChoice run_game_over(const logic::World&)
{
    bn::bg_palettes::set_transparent_color(bn::color(12, 2, 4)); // dark red backdrop

    bn::sprite_text_generator text(common::variable_8x16_sprite_font);
    text.set_center_alignment();

    // Static title.
    bn::vector<bn::sprite_ptr, 24> title;
    text.generate(0, -48, "GAME OVER", title);

    // The two menu rows are redrawn whenever the selection changes so the `>`
    // cursor moves to the highlighted option. Default highlight = Continue.
    int sel = 0; // 0 = Continue, 1 = Quit to title
    bn::vector<bn::sprite_ptr, 40> menu;
    auto draw_menu = [&]{
        menu.clear();
        text.generate(0,  0, sel == 0 ? "> CONTINUE"      : "  CONTINUE",      menu);
        text.generate(0, 20, sel == 1 ? "> QUIT TO TITLE" : "  QUIT TO TITLE", menu);
    };
    draw_menu();

    engine::fade_in(16); // fade in from the black handed over by the previous scene

    while(true)
    {
        if(bn::keypad::up_pressed() && sel != 0)   { sel = 0; draw_menu(); }
        if(bn::keypad::down_pressed() && sel != 1) { sel = 1; draw_menu(); }
        if(bn::keypad::a_pressed() || bn::keypad::start_pressed())
        {
            engine::fade_out(16); // hand the next scene a black screen
            return sel == 0 ? GameOverChoice::Continue : GameOverChoice::QuitToTitle;
        }
        bn::core::update();
    }
}
}
