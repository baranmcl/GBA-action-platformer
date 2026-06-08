#include "engine/input.h"
#include "bn_keypad.h"
namespace engine {
logic::InputFrame read_input(){
    logic::InputFrame f;
    f.left = bn::keypad::left_held();
    f.right = bn::keypad::right_held();
    f.jump_pressed = bn::keypad::a_pressed();   // tap A = jump / double-jump
    f.fire_pressed = bn::keypad::b_pressed();
    f.glide_held   = bn::keypad::a_held();       // hold A = glide (M5)
    return f;
}
}
