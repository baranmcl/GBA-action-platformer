#include "engine/spell_input.h"
#include "bn_keypad.h"
namespace engine {
SpellIntent read_spell_intent(){
    return SpellIntent{ bn::keypad::r_pressed(), bn::keypad::l_pressed() };
}
}
