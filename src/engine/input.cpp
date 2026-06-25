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
    f.down         = bn::keypad::down_held();     // hold Down (+A in air) = Stone ground-pound (M8)
    return f;
}

int read_aim_dy(){
    if(bn::keypad::up_held())   return -14;   // fire HIGH (toward the head)
    if(bn::keypad::down_held()) return 8;     // fire LOW (raised a touch so it clears the floor)
    return 0;                                 // MEDIUM (centre)
}
}
