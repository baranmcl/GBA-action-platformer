#include "engine/pause.h"

#include "bn_core.h"
#include "bn_fixed.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "common_variable_8x16_sprite_font.h"

namespace engine {
namespace {
    // I35 CPU meter: session-wide peak, so a single frame spike anywhere in the playthrough stays
    // visible the next time the player pauses. Only ever touched from inside check_pause's paused
    // loop below (never sampled while unpaused) -> zero cost in normal play.
    bn::fixed s_max_cpu_usage = 0;
}

void check_pause()
{
    if(!bn::keypad::start_pressed()) return;   // only enter on a fresh START press

    bn::sprite_text_generator text(common::variable_8x16_sprite_font);
    text.set_center_alignment();
    bn::vector<bn::sprite_ptr, 16> label;
    text.generate(0, 0, "GAME PAUSED", label);

    // I35: one extra line, "CPU nn% / MAX nn%". bn::core::last_cpu_usage() returns a bn::fixed in
    // [0,1] (fraction of the frame used); (usage * 100).integer() converts to an int percent using
    // only fixed-point arithmetic (IMPL-2: no float). Regenerated only when the displayed numbers
    // actually change, to avoid needless sprite churn while sitting on the pause screen.
    bn::vector<bn::sprite_ptr, 24> cpu_line;
    int last_pct = -1, last_max_pct = -1;
    auto refresh_cpu = [&]{
        bn::fixed usage = bn::core::last_cpu_usage();
        if(usage > s_max_cpu_usage) s_max_cpu_usage = usage;
        int pct = (usage * 100).integer();
        int max_pct = (s_max_cpu_usage * 100).integer();
        if(pct == last_pct && max_pct == last_max_pct) return;
        last_pct = pct; last_max_pct = max_pct;
        bn::string<24> s("CPU ");
        s.append(bn::to_string<4>(pct));
        s.append("% / MAX ");
        s.append(bn::to_string<4>(max_pct));
        s.append("%");
        cpu_line.clear();
        text.generate(0, 20, s, cpu_line);
    };
    refresh_cpu(); // samples the frame just before this pause (the last real gameplay frame)

    // Hold here until START is pressed again. The first update consumes the entry frame so the
    // same press can't immediately unpause; thereafter wait for a new START edge.
    bn::core::update();
    while(! bn::keypad::start_pressed())
    {
        refresh_cpu();
        bn::core::update();
    }
    // `label`/`cpu_line` destroyed here -> the text clears and play resumes on the next frame.
}

}
