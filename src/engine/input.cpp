#include "engine/input.h"
#include "bn_keypad.h"
namespace engine {
logic::InputFrame read_input(){
    logic::InputFrame f;
    f.left = bn::keypad::left_held();
    f.right = bn::keypad::right_held();
    f.jump_pressed = bn::keypad::a_pressed();
    f.fire_pressed = bn::keypad::b_pressed();
    return f;
}
}
