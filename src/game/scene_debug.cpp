#include "game/scene_debug.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "common_variable_8x16_sprite_font.h"
#include "engine/fade.h"

namespace game
{
namespace {
    constexpr int ABILITY_COUNT = 8; // logic::Ability has 8 members (Featherleap..Light)
    // 2-letter mnemonics, ordered to match logic::Ability's enum values exactly (index i = ability i).
    const char* const ABILITY_CODE_UP[ABILITY_COUNT] = { "FE","FI","IC","GL","DA","GR","ST","LI" };
    const char* const ABILITY_CODE_DN[ABILITY_COUNT] = { "fe","fi","ic","gl","da","gr","st","li" };

    // Flattened field index space so cursor movement + value-adjust is one small switch instead of
    // separate per-field input handling: 0 = dungeon, 1..8 = abilities, 9 = spronks.
    constexpr int FIELD_DUNGEON  = 0;
    constexpr int FIELD_ABILITY0 = 1;
    constexpr int FIELD_SPRONKS  = FIELD_ABILITY0 + ABILITY_COUNT; // 9
    constexpr int FIELD_COUNT    = FIELD_SPRONKS + 1;              // 10
}

// Dev-only menu (I35). Not polished UI: a handful of always-visible text lines redrawn on change,
// kept short and few in number to stay well under the GBA's 128-sprite OAM budget (one 8x16
// sprite generator glyph per character with this font) — that's why abilities are shown as a
// single packed line of 2-letter codes (case = on/off) rather than 8 separate "NAME: ON/OFF" rows.
//
// Controls: UP/DOWN moves the field cursor (dungeon / each ability / spronk count); LEFT/RIGHT
// changes the value of whichever field is selected (also toggles the selected ability); START
// launches with the current selections; B cancels back to the title without launching.
DebugLaunch run_debug_select()
{
    bn::bg_palettes::set_transparent_color(bn::color(4, 6, 22));

    bn::sprite_text_generator text(common::variable_8x16_sprite_font);
    text.set_left_alignment();

    bn::vector<bn::sprite_ptr, 24> title;
    text.generate(-112, -72, "DEBUG SELECT (DEV)", title);
    bn::vector<bn::sprite_ptr, 32> help1;
    text.generate(-112, 56, "U/D:FIELD L/R:CHANGE", help1);
    bn::vector<bn::sprite_ptr, 32> help2;
    text.generate(-112, 72, "START:LAUNCH  B:BACK", help2);

    int dungeon = 1;
    uint16_t ability_mask = 0;
    int spronks = 0;
    int field = FIELD_DUNGEON;

    bn::vector<bn::sprite_ptr, 24> dungeon_line;
    bn::vector<bn::sprite_ptr, 40> ability_line;
    bn::vector<bn::sprite_ptr, 24> spronks_line;

    auto draw_dungeon = [&]{
        bn::string<24> s(field == FIELD_DUNGEON ? "> DUNGEON: " : "  DUNGEON: ");
        s.append(bn::to_string<4>(dungeon));
        dungeon_line.clear();
        text.generate(-112, -32, s, dungeon_line);
    };
    auto draw_abilities = [&]{
        bn::string<40> s("AB");
        for(int i = 0; i < ABILITY_COUNT; ++i)
        {
            bool on = (ability_mask >> i) & 1u;
            bool cur = (field == FIELD_ABILITY0 + i);
            s.append(cur ? ">" : " ");
            s.append(on ? ABILITY_CODE_UP[i] : ABILITY_CODE_DN[i]);
        }
        ability_line.clear();
        text.generate(-112, 0, s, ability_line);
    };
    auto draw_spronks = [&]{
        bn::string<24> s(field == FIELD_SPRONKS ? "> SPRONKS: " : "  SPRONKS: ");
        s.append(bn::to_string<4>(spronks));
        spronks_line.clear();
        text.generate(-112, 32, s, spronks_line);
    };
    draw_dungeon(); draw_abilities(); draw_spronks();

    engine::fade_in(16); // fade in from the black handed over by the title screen

    while(true)
    {
        bool changed = false;
        if(bn::keypad::up_pressed())   { field = (field - 1 + FIELD_COUNT) % FIELD_COUNT; changed = true; }
        if(bn::keypad::down_pressed()) { field = (field + 1) % FIELD_COUNT; changed = true; }

        if(field == FIELD_DUNGEON)
        {
            if(bn::keypad::left_pressed()  && dungeon > 1) { --dungeon; changed = true; }
            if(bn::keypad::right_pressed() && dungeon < 9) { ++dungeon; changed = true; }
        }
        else if(field == FIELD_SPRONKS)
        {
            if(bn::keypad::left_pressed()  && spronks > 0) { --spronks; changed = true; }
            if(bn::keypad::right_pressed() && spronks < 8) { ++spronks; changed = true; }
        }
        else // an ability field: any of LEFT/RIGHT/A toggles it
        {
            int idx = field - FIELD_ABILITY0;
            if(bn::keypad::left_pressed() || bn::keypad::right_pressed() || bn::keypad::a_pressed())
            {
                ability_mask ^= (uint16_t)(1u << idx);
                changed = true;
            }
        }

        if(changed) { draw_dungeon(); draw_abilities(); draw_spronks(); }

        if(bn::keypad::b_pressed())
        {
            engine::fade_out(16);
            return DebugLaunch{ logic::World{}, 0 }; // cancelled — caller must not launch
        }
        if(bn::keypad::start_pressed())
        {
            logic::World w = logic::make_debug_world(ability_mask, spronks);
            engine::fade_out(16); // hand the next scene a black screen
            return DebugLaunch{ w, dungeon };
        }

        bn::core::update();
    }
}
}
