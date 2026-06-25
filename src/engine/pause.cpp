#include "engine/pause.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "common_variable_8x16_sprite_font.h"

namespace engine {

void check_pause()
{
    if(!bn::keypad::start_pressed()) return;   // only enter on a fresh START press

    bn::sprite_text_generator text(common::variable_8x16_sprite_font);
    text.set_center_alignment();
    bn::vector<bn::sprite_ptr, 16> label;
    text.generate(0, 0, "GAME PAUSED", label);

    // Hold here until START is pressed again. The first update consumes the entry frame so the
    // same press can't immediately unpause; thereafter wait for a new START edge.
    bn::core::update();
    while(! bn::keypad::start_pressed())
        bn::core::update();
    // `label` destroyed here -> the text clears and play resumes on the next frame.
}

}
